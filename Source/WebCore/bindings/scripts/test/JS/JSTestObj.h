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

#ifndef JSTestObj_h
#define JSTestObj_h

#include "JSDOMWrapper.h"
#include "TestObj.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

class JSTestObj : public JSDOMWrapper {
public:
    typedef JSDOMWrapper Base;
    static JSTestObj* create(JSC::Structure* structure, JSDOMGlobalObject* globalObject, Ref<TestObj>&& impl)
    {
        JSTestObj* ptr = new (NotNull, JSC::allocateCell<JSTestObj>(globalObject->vm().heap)) JSTestObj(structure, globalObject, WTF::move(impl));
        ptr->finishCreation(globalObject->vm());
        return ptr;
    }

    static JSC::JSObject* createPrototype(JSC::VM&, JSC::JSGlobalObject*);
    static JSC::JSObject* getPrototype(JSC::VM&, JSC::JSGlobalObject*);
    static TestObj* toWrapped(JSC::JSValue);
    static bool getOwnPropertySlot(JSC::JSObject*, JSC::ExecState*, JSC::PropertyName, JSC::PropertySlot&);
    static void destroy(JSC::JSCell*);
    ~JSTestObj();

    DECLARE_INFO;

    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

    static JSC::JSValue getConstructor(JSC::VM&, JSC::JSGlobalObject*);
    JSC::WriteBarrier<JSC::Unknown> m_cachedAttribute1;
    JSC::WriteBarrier<JSC::Unknown> m_cachedAttribute2;
    static void visitChildren(JSCell*, JSC::SlotVisitor&);


    // Custom attributes
    JSC::JSValue customAttr(JSC::ExecState&) const;
    void setCustomAttr(JSC::ExecState&, JSC::JSValue);

    // Custom functions
    JSC::JSValue customMethod(JSC::ExecState&);
    JSC::JSValue customMethodWithArgs(JSC::ExecState&);
    static JSC::JSValue classMethod2(JSC::ExecState&);
    TestObj& impl() const { return *m_impl; }
    void releaseImpl() { std::exchange(m_impl, nullptr)->deref(); }

private:
    TestObj* m_impl;
public:
    static const unsigned StructureFlags = JSC::OverridesGetOwnPropertySlot | Base::StructureFlags;
protected:
    JSTestObj(JSC::Structure*, JSDOMGlobalObject*, Ref<TestObj>&&);

    void finishCreation(JSC::VM& vm)
    {
        Base::finishCreation(vm);
        ASSERT(inherits(info()));
    }

};

class JSTestObjOwner : public JSC::WeakHandleOwner {
public:
    virtual bool isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown>, void* context, JSC::SlotVisitor&);
    virtual void finalize(JSC::Handle<JSC::Unknown>, void* context);
};

inline JSC::WeakHandleOwner* wrapperOwner(DOMWrapperWorld&, TestObj*)
{
    static NeverDestroyed<JSTestObjOwner> owner;
    return &owner.get();
}

JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, TestObj*);
inline JSC::JSValue toJS(JSC::ExecState* state, JSDOMGlobalObject* globalObject, TestObj& impl) { return toJS(state, globalObject, &impl); }
JSC::JSValue toJSNewlyCreated(JSC::ExecState*, JSDOMGlobalObject*, TestObj*);

// Functions

JSC::EncodedJSValue JSC_HOST_CALL jsTestObjPrototypeFunctionCustomBindingMethod(JSC::ExecState*);
JSC::EncodedJSValue JSC_HOST_CALL jsTestObjPrototypeFunctionCustomBindingMethodWithArgs(JSC::ExecState*);


} // namespace WebCore

#endif
