#ifndef _AMINOBASE_JS_H
#define _AMINOBASE_JS_H

#include <node.h>
#include <node_buffer.h>
#include <nan.h>
#include <map>

//FIXME
#define DEBUG_BASE true
#define DEBUG_RESOURCES true

class AminoJSObject;

/**
 * Factory object to create JS instance.
 */
class AminoJSObjectFactory {
public:
    std::string name;
    Nan::FunctionCallback callback;

    AminoJSObjectFactory(std::string name, Nan::FunctionCallback callback);

    virtual AminoJSObject* create();
};

/**
 * Basic JS object wrapper for Amino classes.
 *
 * Note: abstract.
 */
class AminoJSObject : public Nan::ObjectWrap {
protected:
    std::string name;

    AminoJSObject(std::string name);
    virtual ~AminoJSObject();

    virtual void setup();

    //properties
    bool addPropertyWatcher(std::string name, int id, v8::Local<v8::Value> &jsValue);

    void updateProperty(std::string name, v8::Local<v8::Value> value);
    void updateProperty(std::string name, double value);
    void updateProperty(std::string name, int value);

    class AnyProperty {
    public:
        AminoJSObject *obj;
        std::string name;
        int id;
        bool connected = false;

        AnyProperty(AminoJSObject *obj, std::string name, int id);
        virtual ~AnyProperty() = 0;

        virtual void setValue(v8::Local<v8::Value> &value);

        void retain();
        void release();
    };

    class FloatProperty : public AnyProperty {
    public:
        float value;

        FloatProperty(AminoJSObject *obj, std::string name, int id);
        ~FloatProperty();

        void setValue(v8::Local<v8::Value> &value) override;
        void setValue(float newValue);
    };

    FloatProperty* createFloatProperty(std::string name);

    //async updates
    void createAsyncQueue();
    void attachToAsyncQueue(AminoJSObject *obj);

    void processAsyncQueue();
    virtual void handleAsyncUpdate(AnyProperty *property, v8::Local<v8::Value> value);

    //static methods
    static v8::Local<v8::FunctionTemplate> createTemplate(AminoJSObjectFactory* factory);
    static void createInstance(Nan::NAN_METHOD_ARGS_TYPE info, AminoJSObjectFactory* factory);

private:
    //properties
    int lastPropertyId = 0;
    std::map<int, AnyProperty *> propertyMap;

    void enqueuePropertyUpdate(int id, v8::Local<v8::Value> value);
    static NAN_METHOD(PropertyUpdated);

    //async updates
    class AsyncUpdate {
    public:
        AnyProperty *property;

        AsyncUpdate(AnyProperty *property, v8::Local<v8::Value> value);
        ~AsyncUpdate();

        v8::Local<v8::Value> getValue();

    private:
        Nan::Persistent<v8::Value> value;
    };

    std::vector<AsyncUpdate *> *asyncUpdates = NULL;
    bool localAsyncUpdatesInstance = false;

public:
    /**
     * Get JS class name.
     *
     * Note: abstract
     */
    std::string getName() {
        return name;
    }
};

#endif