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

#ifndef JSTestCustomNamedGetter_h
#define JSTestCustomNamedGetter_h

#include "JSDOMWrapper.h"
#include "TestCustomNamedGetter.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

class JSTestCustomNamedGetter : public JSDOMWrapper {
public:
    typedef JSDOMWrapper Base;
    static JSTestCustomNamedGetter* create(JSC::Structure* structure, JSDOMGlobalObject* globalObject, Ref<TestCustomNamedGetter>&& impl)
    {
        JSTestCustomNamedGetter* ptr = new (NotNull, JSC::allocateCell<JSTestCustomNamedGetter>(globalObject->vm().heap)) JSTestCustomNamedGetter(structure, globalObject, WTF::move(impl));
        ptr->finishCreation(globalObject->vm());
        return ptr;
    }

    static JSC::JSObject* createPrototype(JSC::VM&, JSC::JSGlobalObject*);
    static JSC::JSObject* getPrototype(JSC::VM&, JSC::JSGlobalObject*);
    static TestCustomNamedGetter* toWrapped(JSC::JSValue);
    static bool getOwnPropertySlot(JSC::JSObject*, JSC::ExecState*, JSC::PropertyName, JSC::PropertySlot&);
    static bool getOwnPropertySlotByIndex(JSC::JSObject*, JSC::ExecState*, unsigned propertyName, JSC::PropertySlot&);
    static void destroy(JSC::JSCell*);
    ~JSTestCustomNamedGetter();

    DECLARE_INFO;

    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

    static JSC::JSValue getConstructor(JSC::VM&, JSC::JSGlobalObject*);
    TestCustomNamedGetter& impl() const { return *m_impl; }
    void releaseImpl() { std::exchange(m_impl, nullptr)->deref(); }

private:
    TestCustomNamedGetter* m_impl;
public:
    static const unsigned StructureFlags = JSC::InterceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero | JSC::OverridesGetOwnPropertySlot | Base::StructureFlags;
protected:
    JSTestCustomNamedGetter(JSC::Structure*, JSDOMGlobalObject*, Ref<TestCustomNamedGetter>&&);

    void finishCreation(JSC::VM& vm)
    {
        Base::finishCreation(vm);
        ASSERT(inherits(info()));
    }

private:
    bool nameGetter(JSC::ExecState*, JSC::PropertyName, JSC::JSValue&);
};

class JSTestCustomNamedGetterOwner : public JSC::WeakHandleOwner {
public:
    virtual bool isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown>, void* context, JSC::SlotVisitor&);
    virtual void finalize(JSC::Handle<JSC::Unknown>, void* context);
};

inline JSC::WeakHandleOwner* wrapperOwner(DOMWrapperWorld&, TestCustomNamedGetter*)
{
    static NeverDestroyed<JSTestCustomNamedGetterOwner> owner;
    return &owner.get();
}

JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, TestCustomNamedGetter*);
inline JSC::JSValue toJS(JSC::ExecState* state, JSDOMGlobalObject* globalObject, TestCustomNamedGetter& impl) { return toJS(state, globalObject, &impl); }
JSC::JSValue toJSNewlyCreated(JSC::ExecState*, JSDOMGlobalObject*, TestCustomNamedGetter*);


} // namespace WebCore

#endif
