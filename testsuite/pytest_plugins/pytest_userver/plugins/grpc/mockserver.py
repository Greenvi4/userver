"""
Mocks for the gRPC servers.

@sa @ref scripts/docs/en/userver/tutorial/grpc_service.md
"""

# pylint: disable=no-member
import asyncio
import contextlib
import functools
from typing import Callable
from typing import List
from typing import Optional

import grpc
import pytest

from testsuite.utils import callinfo

DEFAULT_PORT = 8091

USERVER_CONFIG_HOOKS = ['userver_config_grpc_mockserver']


class GrpcServiceMock:
    def __init__(self, servicer, methods):
        self.servicer = servicer
        self._known_methods = methods
        self._methods = {}

    def get(self, method, default):
        return self._methods.get(method, default)

    def reset_handlers(self):
        self._methods = {}

    @contextlib.contextmanager
    def mock(self):
        try:
            yield self.install_handler
        finally:
            self._methods = {}

    def install_handler(self, method: str):
        def decorator(func):
            if method not in self._known_methods:
                raise RuntimeError(
                    f'Trying to mock unknown grpc method {method}',
                )

            wrapped = callinfo.acallqueue(func)
            self._methods[method] = wrapped
            return wrapped

        return decorator


@pytest.fixture(scope='session')
def grpc_mockserver_endpoint(pytestconfig, get_free_port) -> str:
    """
    Returns the gRPC endpoint to start the mocking server that is set by
    command line `--grpc-mockserver-host` and `--grpc-mockserver-port` options.

    For port 0, picks some free port.

    Override this fixture to customize the endpoint used by gRPC mockserver.

    @snippet samples/grpc_service/tests/conftest.py  Prepare configs
    @ingroup userver_testsuite_fixtures
    """
    port = pytestconfig.option.grpc_mockserver_port
    if pytestconfig.option.service_wait or pytestconfig.option.service_disable:
        port = port or DEFAULT_PORT
    if port == 0:
        port = get_free_port()
    return f'{pytestconfig.option.grpc_mockserver_host}:{port}'


@pytest.fixture(scope='session')
async def grpc_mockserver(grpc_mockserver_endpoint) -> grpc.aio.Server:
    """
    Returns the gRPC mocking server.

    @snippet samples/grpc_service/tests/conftest.py  Prepare server mock
    @ingroup userver_testsuite_fixtures
    """
    server = grpc.aio.server()
    server.add_insecure_port(grpc_mockserver_endpoint)
    await server.start()

    try:
        yield server
    finally:

        async def stop_server():
            await server.stop(grace=None)
            await server.wait_for_termination()

        stop_server_task = asyncio.shield(asyncio.create_task(stop_server()))
        # asyncio.shield does not protect our await from cancellations, and we
        # really need to wait for the server stopping before continuing.
        try:
            await stop_server_task
        except asyncio.CancelledError:
            await stop_server_task
            # Propagate cancellation when we are done
            raise


@pytest.fixture(scope='session')
def userver_config_grpc_mockserver(grpc_mockserver_endpoint):
    """
    Returns a function that adjusts the static config for testsuite.
    Walks through config_vars *values* equal to `$grpc_mockserver`,
    and replaces them with @ref grpc_mockserver_endpoint.

    @ingroup userver_testsuite_fixtures
    """

    def patch_config(_config_yaml, config_vars):
        for name in config_vars:
            if config_vars[name] == '$grpc_mockserver':
                config_vars[name] = grpc_mockserver_endpoint

    return patch_config


@pytest.fixture(scope='session')
def create_grpc_mock():
    """
    Creates the gRPC mock server for the provided type.

    @snippet samples/grpc_service/tests/conftest.py  Prepare server mock
    @ingroup userver_testsuite_fixtures
    """
    return _create_servicer_mock


def pytest_addoption(parser):
    group = parser.getgroup('grpc-mockserver')
    group.addoption(
        '--grpc-mockserver-host',
        default='[::]',
        help='gRPC mockserver hostname, default is [::]',
    )
    group.addoption(
        '--grpc-mockserver-port',
        type=int,
        default=0,
        help='gRPC mockserver port, by default random port is used',
    )


def _create_servicer_mock(
    servicer_class: type,
    *,
    stream_method_names: Optional[List[str]] = None,
    before_call_hook: Optional[Callable[..., None]] = None,
) -> GrpcServiceMock:
    def wrap_grpc_method(name, default_method, is_stream):
        @functools.wraps(default_method)
        async def run_method(self, *args, **kwargs):
            if before_call_hook is not None:
                before_call_hook(*args, **kwargs)

            method = mock.get(name, None)
            if method is not None:
                return await method(*args, **kwargs)
            else:
                return default_method(self, *args, **kwargs)

        @functools.wraps(default_method)
        async def run_stream_method(self, *args, **kwargs):
            if before_call_hook is not None:
                before_call_hook(*args, **kwargs)

            method = mock.get(name, None)
            async for response in await method(*args, **kwargs):
                yield response

        return run_stream_method if is_stream else run_method

    methods = {}
    for attname, value in servicer_class.__dict__.items():
        if callable(value):
            methods[attname] = wrap_grpc_method(
                name=attname,
                default_method=value,
                is_stream=attname in (stream_method_names or []),
            )

    mocked_servicer_class = type(
        f'Mock{servicer_class.__name__}',
        (servicer_class,),
        methods,
    )
    servicer = mocked_servicer_class()
    mock = GrpcServiceMock(servicer, frozenset(methods))
    return mock
