{
    "targets": [{
        "target_name": "testaddon",
        "cflags!": [ "-fno-exceptions" ],
        "cflags_cc!": [ "-fno-exceptions" ],
        "sources": [
            "node-bindings/main.cpp",
            "node-bindings/Samples/functionexample.cpp",
            "node-bindings/Samples/actualclass.cpp",
            "node-bindings/Samples/classexample.cpp",
            "node-bindings/Samples/diff/dmp_diff.hpp"
        ],
        'include_dirs': [
            "<!@(node -p \"require('node-addon-api').include\")"
        ],
        'libraries': [],
        'dependencies': [
            "<!(node -p \"require('node-addon-api').gyp\")"
        ],
        'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ]
    }]
}