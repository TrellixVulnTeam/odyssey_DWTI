/*
    This file is part of the WebKit open source project.
    This file has been generated by generate-bindings.pl. DO NOT MODIFY!

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "JSTestCustomConstructorWithNoInterfaceObject.h"

#include "JSDOMBinding.h"
#include <wtf/GetPtr.h>

using namespace JSC;

namespace WebCore {

// Attributes

JSC::EncodedJSValue jsTestCustomConstructorWithNoInterfaceObjectConstructor(JSC::ExecState*, JSC::JSObject*, JSC::EncodedJSValue, JSC::PropertyName);

class JSTestCustomConstructorWithNoInterfaceObjectPrototype : public JSC::JSNonFinalObject {
public:
    typedef JSC::JSNonFinalObject Base;
    static JSTestCustomConstructorWithNoInterfaceObjectPrototype* create(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::Structure* structure)
    {
        JSTestCustomConstructorWithNoInterfaceObjectPrototype* ptr = new (NotNull, JSC::allocateCell<JSTestCustomConstructorWithNoInterfaceObjectPrototype>(vm.heap)) JSTestCustomConstructorWithNoInterfaceObjectPrototype(vm, globalObject, structure);
        ptr->finishCreation(vm);
        return ptr;
    }

    DECLARE_INFO;
    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

private:
    JSTestCustomConstructorWithNoInterfaceObjectPrototype(JSC::VM& vm, JSC::JSGlobalObject*, JSC::Structure* structure)
        : JSC::JSNonFinalObject(vm, structure)
    {
    }

    void finishCreation(JSC::VM&);
};

class JSTestCustomConstructorWithNoInterfaceObjectConstructor : public DOMConstructorObject {
private:
    JSTestCustomConstructorWithNoInterfaceObjectConstructor(JSC::Structure*, JSDOMGlobalObject*);
    void finishCreation(JSC::VM&, JSDOMGlobalObject*);

public:
    typedef DOMConstructorObject Base;
    static JSTestCustomConstructorWithNoInterfaceObjectConstructor* create(JSC::VM& vm, JSC::Structure* structure, JSDOMGlobalObject* globalObject)
    {
        JSTestCustomConstructorWithNoInterfaceObjectConstructor* ptr = new (NotNull, JSC::allocateCell<JSTestCustomConstructorWithNoInterfaceObjectConstructor>(vm.heap)) JSTestCustomConstructorWithNoInterfaceObjectConstructor(structure, globalObject);
        ptr->finishCreation(vm, globalObject);
        return ptr;
    }

    DECLARE_INFO;
    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }
    static JSC::ConstructType getConstructData(JSC::JSCell*, JSC::ConstructData&);
};

const ClassInfo JSTestCustomConstructorWithNoInterfaceObjectConstructor::s_info = { "TestCustomConstructorWithNoInterfaceObjectConstructor", &Base::s_info, 0, CREATE_METHOD_TABLE(JSTestCustomConstructorWithNoInterfaceObjectConstructor) };

JSTestCustomConstructorWithNoInterfaceObjectConstructor::JSTestCustomConstructorWithNoInterfaceObjectConstructor(Structure* structure, JSDOMGlobalObject* globalObject)
    : Base(structure, globalObject)
{
}

void JSTestCustomConstructorWithNoInterfaceObjectConstructor::finishCreation(VM& vm, JSDOMGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    putDirect(vm, vm.propertyNames->prototype, JSTestCustomConstructorWithNoInterfaceObject::getPrototype(vm, globalObject), DontDelete | ReadOnly | DontEnum);
    putDirect(vm, vm.propertyNames->name, jsNontrivialString(&vm, String(ASCIILiteral("TestCustomConstructorWithNoInterfaceObject"))), ReadOnly | DontEnum);
    putDirect(vm, vm.propertyNames->length, jsNumber(0), ReadOnly | DontEnum);
}

ConstructType JSTestCustomConstructorWithNoInterfaceObjectConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructJSTestCustomConstructorWithNoInterfaceObject;
    return ConstructTypeHost;
}

/* Hash table for prototype */

static const HashTableValue JSTestCustomConstructorWithNoInterfaceObjectPrototypeTableValues[] =
{
    { "constructor", DontEnum | ReadOnly, NoIntrinsic, { (intptr_t)static_cast<PropertySlot::GetValueFunc>(jsTestCustomConstructorWithNoInterfaceObjectConstructor), (intptr_t) static_cast<PutPropertySlot::PutValueFunc>(0) } },
};

const ClassInfo JSTestCustomConstructorWithNoInterfaceObjectPrototype::s_info = { "TestCustomConstructorWithNoInterfaceObjectPrototype", &Base::s_info, 0, CREATE_METHOD_TABLE(JSTestCustomConstructorWithNoInterfaceObjectPrototype) };

void JSTestCustomConstructorWithNoInterfaceObjectPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    reifyStaticProperties(vm, JSTestCustomConstructorWithNoInterfaceObjectPrototypeTableValues, *this);
}

const ClassInfo JSTestCustomConstructorWithNoInterfaceObject::s_info = { "TestCustomConstructorWithNoInterfaceObject", &Base::s_info, 0, CREATE_METHOD_TABLE(JSTestCustomConstructorWithNoInterfaceObject) };

JSTestCustomConstructorWithNoInterfaceObject::JSTestCustomConstructorWithNoInterfaceObject(Structure* structure, JSDOMGlobalObject* globalObject, Ref<TestCustomConstructorWithNoInterfaceObject>&& impl)
    : JSDOMWrapper(structure, globalObject)
    , m_impl(&impl.leakRef())
{
}

JSObject* JSTestCustomConstructorWithNoInterfaceObject::createPrototype(VM& vm, JSGlobalObject* globalObject)
{
    return JSTestCustomConstructorWithNoInterfaceObjectPrototype::create(vm, globalObject, JSTestCustomConstructorWithNoInterfaceObjectPrototype::createStructure(vm, globalObject, globalObject->objectPrototype()));
}

JSObject* JSTestCustomConstructorWithNoInterfaceObject::getPrototype(VM& vm, JSGlobalObject* globalObject)
{
    return getDOMPrototype<JSTestCustomConstructorWithNoInterfaceObject>(vm, globalObject);
}

void JSTestCustomConstructorWithNoInterfaceObject::destroy(JSC::JSCell* cell)
{
    JSTestCustomConstructorWithNoInterfaceObject* thisObject = static_cast<JSTestCustomConstructorWithNoInterfaceObject*>(cell);
    thisObject->JSTestCustomConstructorWithNoInterfaceObject::~JSTestCustomConstructorWithNoInterfaceObject();
}

JSTestCustomConstructorWithNoInterfaceObject::~JSTestCustomConstructorWithNoInterfaceObject()
{
    releaseImpl();
}

EncodedJSValue jsTestCustomConstructorWithNoInterfaceObjectConstructor(ExecState* state, JSObject* baseValue, EncodedJSValue, PropertyName)
{
    JSTestCustomConstructorWithNoInterfaceObjectPrototype* domObject = jsDynamicCast<JSTestCustomConstructorWithNoInterfaceObjectPrototype*>(baseValue);
    if (!domObject)
        return throwVMTypeError(state);
    JSValue constructor = JSTestCustomConstructorWithNoInterfaceObjectConstructor::create(state->vm(), JSTestCustomConstructorWithNoInterfaceObjectConstructor::createStructure(state->vm(), domObject->globalObject(), domObject->globalObject()->objectPrototype()), jsCast<JSDOMGlobalObject*>(domObject->globalObject()));
    // Shadowing constructor property to ensure reusing the same constructor object
    domObject->putDirect(state->vm(), state->propertyNames().constructor, constructor, DontEnum | ReadOnly);
    return JSValue::encode(constructor);
}

bool JSTestCustomConstructorWithNoInterfaceObjectOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, SlotVisitor& visitor)
{
    UNUSED_PARAM(handle);
    UNUSED_PARAM(visitor);
    return false;
}

void JSTestCustomConstructorWithNoInterfaceObjectOwner::finalize(JSC::Handle<JSC::Unknown> handle, void* context)
{
    auto* jsTestCustomConstructorWithNoInterfaceObject = jsCast<JSTestCustomConstructorWithNoInterfaceObject*>(handle.slot()->asCell());
    auto& world = *static_cast<DOMWrapperWorld*>(context);
    uncacheWrapper(world, &jsTestCustomConstructorWithNoInterfaceObject->impl(), jsTestCustomConstructorWithNoInterfaceObject);
}

#if ENABLE(BINDING_INTEGRITY)
#if PLATFORM(WIN)
#pragma warning(disable: 4483)
extern "C" { extern void (*const __identifier("??_7TestCustomConstructorWithNoInterfaceObject@WebCore@@6B@")[])(); }
#else
extern "C" { extern void* _ZTVN7WebCore42TestCustomConstructorWithNoInterfaceObjectE[]; }
#endif
#endif

JSC::JSValue toJSNewlyCreated(JSC::ExecState*, JSDOMGlobalObject* globalObject, TestCustomConstructorWithNoInterfaceObject* impl)
{
    if (!impl)
        return jsNull();
    return createNewWrapper<JSTestCustomConstructorWithNoInterfaceObject>(globalObject, impl);
}

JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject* globalObject, TestCustomConstructorWithNoInterfaceObject* impl)
{
    if (!impl)
        return jsNull();
    if (JSValue result = getExistingWrapper<JSTestCustomConstructorWithNoInterfaceObject>(globalObject, impl))
        return result;

#if ENABLE(BINDING_INTEGRITY)
    void* actualVTablePointer = *(reinterpret_cast<void**>(impl));
#if PLATFORM(WIN)
    void* expectedVTablePointer = reinterpret_cast<void*>(__identifier("??_7TestCustomConstructorWithNoInterfaceObject@WebCore@@6B@"));
#else
    void* expectedVTablePointer = &_ZTVN7WebCore42TestCustomConstructorWithNoInterfaceObjectE[2];
#if COMPILER(CLANG)
    // If this fails TestCustomConstructorWithNoInterfaceObject does not have a vtable, so you need to add the
    // ImplementationLacksVTable attribute to the interface definition
    COMPILE_ASSERT(__is_polymorphic(TestCustomConstructorWithNoInterfaceObject), TestCustomConstructorWithNoInterfaceObject_is_not_polymorphic);
#endif
#endif
    // If you hit this assertion you either have a use after free bug, or
    // TestCustomConstructorWithNoInterfaceObject has subclasses. If TestCustomConstructorWithNoInterfaceObject has subclasses that get passed
    // to toJS() we currently require TestCustomConstructorWithNoInterfaceObject you to opt out of binding hardening
    // by adding the SkipVTableValidation attribute to the interface IDL definition
    RELEASE_ASSERT(actualVTablePointer == expectedVTablePointer);
#endif
    return createNewWrapper<JSTestCustomConstructorWithNoInterfaceObject>(globalObject, impl);
}

TestCustomConstructorWithNoInterfaceObject* JSTestCustomConstructorWithNoInterfaceObject::toWrapped(JSC::JSValue value)
{
    if (auto* wrapper = jsDynamicCast<JSTestCustomConstructorWithNoInterfaceObject*>(value))
        return &wrapper->impl();
    return nullptr;
}

}
