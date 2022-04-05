#
# This file is distributed under the MIT License. See LICENSE.md for details.
#

import copy
from abc import ABC
from typing import Dict, List

from .definition import Definition

# TODO: should we return the fields in a stable order (in all_fields, all_optional_fields, etc)


class StructField(ABC):
    def __init__(self, *, name, doc=None, optional=False, const=False, is_guid=False):
        self.name = name
        self.doc = doc
        self.optional = optional
        self.const = const
        self.is_guid = is_guid

    @staticmethod
    def from_yaml(source_dict: Dict):
        if source_dict.get("type"):
            return SimpleStructField(**source_dict)

        elif source_dict.get("sequence"):
            sequence_def = source_dict["sequence"]
            args = copy.copy(source_dict)
            del args["sequence"]
            args.update(
                {
                    "sequence_type": sequence_def["type"],
                    "element_type": sequence_def["elementType"],
                    "upcastable": sequence_def.get("upcastable", False),
                }
            )
            return SequenceStructField(**args)

        elif source_dict.get("reference"):
            reference = source_dict["reference"]
            args = copy.copy(source_dict)
            del args["reference"]
            args.update(
                {
                    "pointee_type": reference["pointeeType"],
                    "root_type": reference["rootType"],
                }
            )
            return ReferenceStructField(**args)

        else:
            raise ValueError("Invalid struct field")

    def resolve_references(self, schema):
        raise NotImplementedError()


class SimpleStructField(StructField):
    def __init__(self, *, name, type, doc=None, optional=False, const=False, is_guid=False):
        super().__init__(name=name, doc=doc, optional=optional, const=const, is_guid=is_guid)
        self.type = type
        self.resolved_type = None

    def resolve_references(self, schema):
        self.resolved_type = schema.get_definition_for(self.type)

    @property
    def python_type(self):
        if self.resolved_type:
            return self.resolved_type.name

        return _scalar_python_type(self.type)


class SequenceStructField(StructField):
    def __init__(
        self,
        *,
        name,
        sequence_type,
        element_type,
        upcastable=False,
        doc=None,
        optional=False,
        const=False,
    ):
        super().__init__(name=name, doc=doc, optional=optional, const=const)
        self.sequence_type = sequence_type
        self.element_type = element_type
        self.upcastable = upcastable
        # TODO: does it make sense to try to resolve the sequence type?
        self.resolved_sequence_type = None
        self.resolved_element_type = None

    @property
    def type(self):
        if self.upcastable:
            return f"{self.sequence_type}<UpcastablePointer<{self.element_type}>>"
        else:
            return f"{self.sequence_type}<{self.element_type}>"

    @property
    def python_type(self):
        if self.resolved_element_type:
            return f"List[{self.resolved_element_type.name}]"
        element_type = _scalar_python_type(self.element_type)
        return f"List[{element_type}]"

    def resolve_references(self, schema):
        self.resolved_sequence_type = schema.get_definition_for(self.sequence_type)
        self.resolved_element_type = schema.get_definition_for(self.element_type)


class ReferenceStructField(StructField):
    def __init__(self, *, name, pointee_type, root_type, doc=None, optional=False, const=False):
        super().__init__(name=name, doc=doc, optional=optional, const=const)
        self.pointee_type = pointee_type
        self.root_type = root_type
        self.resolved_pointee_type = None
        self.resolved_root_type = None

    @property
    def type(self):
        return f"TupleTreeReference<{self.pointee_type}, {self.root_type}>"

    @property
    def python_type(self):
        if self.resolved_pointee_type:
            pointee_type = self.resolved_pointee_type.name
        else:
            pointee_type = _scalar_python_type(self.pointee_type)

        if self.resolved_root_type:
            root_type = self.resolved_root_type.name
        else:
            root_type = _scalar_python_type(self.root_type)

        return f"Reference[{pointee_type}, {root_type}]"

    def resolve_references(self, schema):
        self.resolved_pointee_type = schema.get_definition_for(self.pointee_type)
        self.resolved_root_type = schema.get_definition_for(self.root_type)


class StructDefinition(Definition):
    def __init__(
        self,
        *,
        namespace,
        user_namespace,
        name,
        fields,
        inherits=None,
        doc=None,
        tag=None,
        abstract=False,
        _key=None,
    ):
        super(StructDefinition, self).__init__(
            namespace,
            user_namespace,
            name,
            f"{namespace}::{name}",
            f"{user_namespace}::{name}",
            doc=doc,
        )
        self.fields: List[StructField] = fields
        self.abstract = abstract
        self._key = _key
        self._inherits = inherits

        # These fields will be populated by resolve_references()
        self.inherits = None
        # Type containing the key fields (might be self or a base class)
        self.key_definition = None
        # None, "simple" or "composite"
        self.keytype = None
        self.key_fields: List[SimpleStructField] = []
        self.emit_full_constructor = None
        self._refs_resolved = False

    def resolve_references(self, schema):
        if self._refs_resolved:
            return

        if self._inherits:
            self.inherits = schema.get_definition_for(self._inherits)
            self.dependencies.add(self._inherits)

        for field in self.fields:
            field.resolve_references(schema)

            if isinstance(field, SimpleStructField):
                self.dependencies.add(field.type)
            elif isinstance(field, SequenceStructField):
                self.dependencies.add(field.sequence_type)
                self.dependencies.add(field.element_type)
            elif isinstance(field, ReferenceStructField):
                self.dependencies.add(field.pointee_type)
                # TODO: if we add this dependency we generate circular dependencies
                # self.dependencies.add(field.root_type)
            else:
                raise ValueError()

        # Register which headers we need
        self.includes.add("Generated/ForwardDecls.h")
        for dep in self.dependencies:
            dep_definition = schema.get_definition_for(dep)
            if dep_definition:
                self.includes.add(dep_definition.filename)

        if self._key:
            self.key_definition = self

        elif self.inherits:
            # TODO: support indirect inheritance
            self.key_definition = self.inherits

        # TODO: mark key fields as const
        if self.key_definition:
            for key_field_name in self.key_definition._key:
                key_field = next(f for f in self.key_definition.fields if f.name == key_field_name)
                assert isinstance(key_field, SimpleStructField)
                self.key_fields.append(key_field)

            if len(self.key_definition.key_fields) == 0:
                self.keytype = None
            elif len(self.key_definition.key_fields) == 1:
                self.keytype = "simple"
            else:
                self.keytype = "composite"

        self._refs_resolved = True

        key_fields_names = {f.name for f in self.key_fields}
        all_field_names = {f.name for f in self.all_fields}
        self.emit_full_constructor = key_fields_names != all_field_names

    @staticmethod
    def from_dict(source_dict: Dict, default_base_namespace: str):
        args = {
            "namespace": source_dict.get("namespace", f"{default_base_namespace}::generated"),
            "user_namespace": source_dict.get("user_namespace", default_base_namespace),
            "name": source_dict["name"],
            "doc": source_dict.get("doc"),
            "fields": [StructField.from_yaml(d) for d in source_dict["fields"]],
            "_key": source_dict.get("key"),
            "inherits": source_dict.get("inherits"),
            "abstract": source_dict.get("abstract", False),
        }
        return StructDefinition(**args)

    @property
    def optional_fields(self):
        for field in self.fields:
            if field.optional:
                yield field

    @property
    def required_fields(self):
        for field in self.fields:
            if not field.optional:
                yield field

    @property
    def all_optional_fields(self):
        for field in self.all_fields:
            if field.optional:
                yield field

    @property
    def all_required_fields(self):
        for field in self.all_fields:
            if not field.optional:
                yield field

    @property
    def all_fields(self):
        if not self._refs_resolved:
            raise RuntimeError("all_fields not available before resolving references")
        if self.inherits:
            yield from self.inherits.fields
        yield from self.fields


def _scalar_python_type(t: str) -> str:
    if "int" in t:
        return "int"
    elif "string" in t:
        return "str"
    return t