#
# This file is distributed under the MIT License. See LICENSE.md for details.
#

import logging
import os
from pathlib import Path
from typing import Optional

from starlette.applications import Starlette
from starlette.config import Config
from starlette.middleware import Middleware
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.middleware.cors import CORSMiddleware
from starlette.requests import Request
from starlette.responses import PlainTextResponse

from ariadne.asgi import GraphQL
from ariadne.contrib.tracing.apollotracing import ApolloTracingExtension

from revng.api import Manager
from revng.api._capi import initialize as capi_initialize

from .demo_webpage import demo_page, production_demo_page
from .manager import make_manager
from .schema_generator import SchemaGen
from .util import project_workdir

workdir: Path = project_workdir()
manager: Optional[Manager] = None
startup_done = False

config = Config()
DEBUG = config("STARLETTE_DEBUG", cast=bool, default=False)


class ManagerMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next):
        assert manager, "Manager not initialized"

        request.scope["workdir"] = workdir
        request.scope["manager"] = manager

        # Safety checks
        assert request.scope["workdir"] is not None
        assert request.scope["workdir"].exists() and request.scope["workdir"].is_dir()
        assert request.scope["manager"] is not None

        return await call_next(request)


async def status(request):
    if startup_done:
        return PlainTextResponse("OK")
    else:
        return PlainTextResponse("KO", 503)


def startup():
    global manager, startup_done
    capi_initialize()
    manager = make_manager(workdir)
    app.mount(
        "/graphql",
        GraphQL(
            SchemaGen().get_schema(manager),
            context_value={"manager": manager, "workdir": workdir},
            extensions=[ApolloTracingExtension],
            debug=DEBUG,
        ),
    )
    startup_done = True


def shutdown():
    if manager is not None:
        store_result = manager.save()
        if not store_result:
            logging.warning("Failed to store manager's containers")


app = Starlette(
    debug=DEBUG,
    middleware=[
        Middleware(ManagerMiddleware),
        Middleware(
            CORSMiddleware,
            allow_origins=os.environ["REVNG_ORIGINS"].split(",")
            if "REVNG_ORIGINS" in os.environ
            else [],
            allow_methods=["*"],
        ),
    ],
    on_startup=[startup],
    on_shutdown=[shutdown],
)


app.add_route("/status", status, ["GET"])
if DEBUG:
    app.add_route("/", demo_page, ["GET"])
else:
    app.add_route("/", production_demo_page, ["GET"])