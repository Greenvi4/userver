## Easy - library for single file prototyping

**Quality:** @ref QUALITY_TIERS "Silver Tier".

`Easy` is the userver library for easy prototyping. Service functionality is described in code in a
short and declarative way. Static configs and database schema are applied
automatically.

Migration of a service on easy library to a more functional
[pg_service_template](https://github.com/userver-framework/pg_service_template)
is straightforward.


### `Hello world` service with the easy library

Let's write a service that responds `Hello world` to any request by URL `/hello`. To do that, just describe the handler
in `main.cpp` file:

@include libraries/easy/samples/0_hello_world/main.cpp

Build it with a trivial `CMakeLists.txt`:

@include libraries/easy/samples/0_hello_world/CMakeLists.txt

Note the `userver_testsuite_add_simple(DUMP_CONFIG True)` usage. It automatically adds the `testsuite` directory
and tells the testsuite to retrieve static config from the binary itself, so
that the new service can be easily tested from python. Just add a `testsuite/conftest.py` file:
@include libraries/easy/samples/0_hello_world/testsuite/conftest.py

And put the tests in any other file, for example `testsuite/test_basic.py`:
@include libraries/easy/samples/0_hello_world/testsuite/test_basic.py

Tests can be run in a usual way, for example
```
# bash
make -j11 userver-easy-samples-hello-world && (cd libraries/easy/samples/0_hello_world && ctest -V)
```

The easy library works well with any callables, so feel free to move the logic out of lambdas to functions or functional
objects:

@include libraries/easy/samples/1_hi/main.cpp


### `Key-Value storage` service with the easy library

Let's write a service that stores a key if it was HTTP POSTed by URL `/kv` with URL arguments `key` and `value`. For
example HTTP POST `/kv?key=the_key&value=the_value` store the `the_value` by key `the_key`. To retrieve a key, an
HTTP GET request for the URL `/kv?key=KEY` is done.

For this service we will need a database. To add a database to the service an easy::PgDep dependency should be added to
the easy::HttpWith. After that, the dependency can be retrieved in the handlers:

@include libraries/easy/samples/2_key_value/main.cpp

Note the easy::HttpWith::DbSchema usage. PostgreSQL database requires schema, so it is provided in place.

To test the service we should instruct the testsuite to retrieve the schema from the service.
Content of `testsuite/conftest.py` file is:
@include libraries/easy/samples/2_key_value/testsuite/conftest.py

After that tests can be written in a usual way, for example `testsuite/test_basic.py`:
@include libraries/easy/samples/2_key_value/testsuite/test_basic.py


### JSON service with the easy library

As you may noticed from a previous example, passing arguments in an URL may not be comfortable, especially when the
request is complex. In this example, let's change the previous service to accept JSON request and answers with JSON, and
force the keys to be integers.

If the function for a path accepts formats::json::Value the easy library attempts to automatically parse the request
as JSON. If the function returns formats::json::Value, then the content type is automatically set to `application/json`:

@include libraries/easy/samples/3_json/main.cpp

Note the `request_json.As<schemas::KeyRequest>()` usage. This example uses @ref scripts/docs/en/userver/chaotic.md
to generate the parsers and serializers via `CMakeLists.txt`:

@include libraries/easy/samples/3_json/CMakeLists.txt

Content of `testsuite/conftest.py` file did not change, the `testsuite/test_basic.py` now uses JSON:
@include libraries/easy/samples/3_json/testsuite/test_basic.py


### Custom and multiple dependencies for a service with the easy library

When the logic of your service becomes complicated and big it is a natural desire to move parts of the logic into a
separate entities. @ref scripts/docs/en/userver/component_system.md "Component system" is the solution for userver
based services.

Consider the example, where an HTTP client to a remote service is converted to a component:

@snippet libraries/easy/samples/4_custom_dependency/main.cpp  ActionClient

To use that component with the easy library a dependency type should be written, that has a constructor from a single
`const components::ComponentContext&` parameter to retrieve the clients/components and has a
`static void RegisterOn(easy::HttpBase& app)` member function, to register the required component and configs in the
component list.

@snippet libraries/easy/samples/4_custom_dependency/main.cpp  ActionDep

@note It is time to think about migrating to a more functional
      [pg_service_template](https://github.com/userver-framework/pg_service_template) if new components start to appear
      in the service written via the easy library.

Multiple dependencies can be used with easy::HttpWith in the following way:

@snippet libraries/easy/samples/4_custom_dependency/main.cpp  main

See the full example, including tests:
* @ref libraries/easy/samples/4_custom_dependency/main.cpp
* @ref libraries/easy/samples/4_custom_dependency/CMakeLists.txt
* @ref libraries/easy/samples/4_custom_dependency/testsuite/conftest.py
* @ref libraries/easy/samples/4_custom_dependency/testsuite/test_basic.py

----------

@htmlonly <div class="bottom-nav"> @endhtmlonly
⇦ @ref clickhouse_driver | @ref scripts/docs/en/userver/libraries/s3api.md ⇨
@htmlonly </div> @endhtmlonly

@example libraries/easy/samples/4_custom_dependency/main.cpp
@example libraries/easy/samples/4_custom_dependency/CMakeLists.txt
@example libraries/easy/samples/4_custom_dependency/testsuite/conftest.py
@example libraries/easy/samples/4_custom_dependency/testsuite/test_basic.py