#include "functionexample.h"
#include "json.hpp"
#include <sstream>
#include <string>

using json = nlohmann::json;

std::string functionexample::hello(std::string a, std::string b)
{
    json document = R"(
         {
           "Color": "White",
           "hello": "world",
           "Animal" : "Dog"
         }
     )"_json;

    // The patch
    json patch = R"(
         [
           { "op": "replace", "path": "/Color", "value": "Black" },
           { "op": "add", "path": "/Cpp", "value": "Secrets" },
           { "op": "remove", "path": "/hello"},
           { "op": "replace", "path": "/Animal","value": "Cat"}
           
         ]
     )"_json;

    // applying the patch
    // json patched_doc = document.patch(patch);

    std::stringstream stream;
    // std::ostringstream stream;

    stream << patch;

    std::string result = stream.str();

    // std::cout << patch << " is a " << patch.type_name() << '\n';

    return result;
}

int functionexample::add(int a, int b)
{
    return a + b;
}

Napi::String functionexample::HelloWrapped(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    Napi::String first = info[0].As<Napi ::String>();
    Napi::String second = info[1].As<Napi ::String>();

    Napi::String returnValue = Napi::String::New(env, functionexample::hello(first, second));
    return returnValue;
}

Napi::Number functionexample::AddWrapped(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber())
    {
        Napi::TypeError::New(env, "Number expected").ThrowAsJavaScriptException();
    }

    Napi::Number first = info[0].As<Napi::Number>();
    Napi::Number second = info[1].As<Napi::Number>();

    int returnValue = functionexample::add(first.Int32Value(), second.Int32Value());

    return Napi::Number::New(env, returnValue);
}

Napi::Object functionexample::Init(Napi::Env env, Napi::Object exports)
{
    exports.Set("hello", Napi::Function::New(env, functionexample::HelloWrapped));
    exports.Set("add", Napi::Function::New(env, functionexample::AddWrapped));
    return exports;
}
