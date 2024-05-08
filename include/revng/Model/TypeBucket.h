#pragma once

//
// This file is distributed under the MIT License. See LICENSE.mit for details.
//

#include "revng/Model/Binary.h"

namespace model {

/// This is simple type container that allows keeping types connected to
/// a specific binary without inserting them in.
class TypeBucket {
private:
  using TypeVector = llvm::SmallVector<UpcastablePointer<model::Type>, 16>;

private:
  TypeVector Types;
  model::Binary &Binary;
  uint64_t FirstAvailableTypeID;
  uint64_t NextAvailableTypeID;

public:
  TypeBucket(model::Binary &Binary) :
    Binary(Binary),
    FirstAvailableTypeID(Binary.getAvailableTypeID()),
    NextAvailableTypeID(FirstAvailableTypeID) {}
  TypeBucket(const TypeBucket &Another) = delete;
  TypeBucket(TypeBucket &&Another) = default;
  ~TypeBucket() {
    revng_assert(Types.empty(),
                 "The bucket has to be explicitly committed or dropped.");
  }

  /// Marks all the current dependencies as essential and merges them into
  /// the managed model.
  ///
  /// The managed model is the one \ref Binary points to.
  void commit() {
    revng_assert(Binary.getAvailableTypeID() == FirstAvailableTypeID,
                 "Unable to commit: owner's id requirements have changed");

    Binary.recordNewTypes(std::move(Types));
    NextAvailableTypeID = FirstAvailableTypeID = Binary.getAvailableTypeID();
    Types.clear();
  }

  /// Aborts dependency management and discards any modification to
  /// the managed model made in the \ref Types vector.
  ///
  /// The managed model is the one \ref Binary points to.
  void drop() {
    NextAvailableTypeID = FirstAvailableTypeID;
    Types.clear();
  }

  [[nodiscard]] bool empty() const { return Types.empty(); }

public:
  /// A helper for new type creation.
  ///
  /// Its usage mirrors that of `model::Binary::makeType<NewType>()`.
  ///
  /// \note This function forcefully assign a new type ID.
  template<typename NewType, typename... Ts>
  [[nodiscard]] std::pair<NewType &, model::TypePath> makeType(Ts &&...As) {
    using UT = model::UpcastableType;
    auto &Ptr = Types.emplace_back(UT::make<NewType>(std::forward<Ts>(As)...));
    NewType *Upcasted = llvm::cast<NewType>(Ptr.get());

    // Assign the type ID
    revng_assert(Upcasted->ID() == 0);
    Upcasted->ID() = NextAvailableTypeID;
    NextAvailableTypeID++;

    model::TypePath ResultPath = Binary.getTypePath(Upcasted->key());
    return { *Upcasted, ResultPath };
  }

public:
  /// A helper for primitive type selection when binary access is limited.
  ///
  /// It mirrors `model::Binary::makeType`.
  inline model::TypePath getPrimitiveType(model::PrimitiveTypeKind::Values Kind,
                                          uint8_t Size) {
    return Binary.getPrimitiveType(Kind, Size);
  }

  /// A helper streamlining selection of types for registers.
  ///
  /// \param Register Any CPU register the model is aware of.
  ///
  /// \return A primitive type in \ref Binary.
  inline model::TypePath defaultRegisterType(model::Register::Values Register) {
    return getPrimitiveType(model::Register::primitiveKind(Register),
                            model::Register::getSize(Register));
  }

  /// A helper streamlining selection of types for registers.
  ///
  /// \param Register Any CPU register the model is aware of.
  ///
  /// \return A primitive type in \ref Binary.
  inline model::TypePath genericRegisterType(model::Register::Values Register) {
    return getPrimitiveType(model::PrimitiveTypeKind::Generic,
                            model::Register::getSize(Register));
  }
};

} // namespace model
