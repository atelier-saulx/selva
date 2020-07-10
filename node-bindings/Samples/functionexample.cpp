#include "functionexample.h"
// #include "json.hpp"
// #include <string>

// using json = nlohmann::json;
// json patch = json::diff(json::parse(a), json::parse(b));
// return patch.dump();
#include <typeinfo> // operator typeid

#include "dmp_diff.hpp"
#include <string>

using namespace std;
using MyersStringDiff = MyersDiff<string>;

// need to return some kind of struct prob
std::string functionexample::hello(std::string a, std::string b)
{
    MyersStringDiff diff{a, b};
    bool isStart = true;
    std::string greeting = "[";
    // can pass stats as well!
    for (const auto &i : diff)
    {
        if (!isStart)
        {
            greeting.append(",");
        }
        if (i.operation == 0)
        {
            // dont add if last
            greeting.append("[");
            // greeting.append("\"");
            // greeting.append(i.str());
            // greeting.append("\"");
            // greeting.append(",");
            greeting.append(to_string(i.text.size()));
            greeting.append("]");
        }
        else if (i.operation == 2)
        {
            greeting.append("[");
            greeting.append(to_string(i.text.size()));
            greeting.append(",");
            greeting.append(to_string(i.operation));
            // for logging! greeting.push_back(op2chr(i.operation));
            greeting.append("]");
        }
        else
        {
            greeting.append("[\"");
            // needs to be escaped!
            greeting.append(i.str());
            greeting.append("\"");
            greeting.append(",");
            greeting.append(to_string(i.operation));
            // for logging! greeting.push_back(op2chr(i.operation));
            greeting.append("]");
        }
        isStart = false;
    }
    greeting.append("]");
    return greeting;
}

int functionexample::add(int a, int b)
{
    return a + b;
}

// napi type
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
