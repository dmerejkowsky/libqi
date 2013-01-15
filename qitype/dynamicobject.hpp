#pragma once
/*
**  Copyright (C) 2012 Aldebaran Robotics
**  See COPYING for the license
*/

#ifndef _QITYPE_DYNAMICOBJECT_HPP_
#define _QITYPE_DYNAMICOBJECT_HPP_

#include <qitype/genericobject.hpp>

#ifdef _MSC_VER
#  pragma warning( push )
#  pragma warning( disable: 4251 )
#endif

namespace qi
{

  class DynamicObjectPrivate;

  /** A Dynamic object is an object that handles all signal/method operation
  * itself.
  *
  * Signal handling:
  * The default implementation is creating a SignalBase for each MetaSignal in
  * the MetaObject, and bounces metaPost(), connect() and disconnect() to it.
  *
  * Method handling:
  * The default implementation holds a method list that the user must populate
  * with setMethod()
  */
  class QITYPE_API DynamicObject {
  public:
    DynamicObject();

    virtual ~DynamicObject();

    /// You *must* call DynamicObject::setMetaObject() if you overload this method.
    virtual void setMetaObject(const MetaObject& mo);


    MetaObject &metaObject();

    void setMethod(unsigned int id, GenericFunction callable);
    GenericFunction method(unsigned int id);
    SignalBase* signalBase(unsigned int id) const;

    virtual qi::Future<GenericValuePtr> metaCall(Manageable* context, unsigned int method, const GenericFunctionParameters& params, MetaCallType callType = MetaCallType_Auto);
    virtual void metaPost(Manageable* context, unsigned int event, const GenericFunctionParameters& params);
    /// Calls given functor when event is fired. Takes ownership of functor.
    virtual qi::Future<unsigned int> metaConnect(unsigned int event, const SignalSubscriber& subscriber);
    /// Disconnect an event link. Returns if disconnection was successful.
    virtual qi::Future<void> metaDisconnect(unsigned int linkId);

    // C4251
    boost::shared_ptr<DynamicObjectPrivate> _p;
  };

  //Make an GenericObject of DynamicObject kind from a DynamicObject
  QITYPE_API ObjectPtr     makeDynamicObjectPtr(DynamicObject *obj, bool destroyObject = true);

}

#ifdef _MSC_VER
#  pragma warning( pop )
#endif

#endif  // _QITYPE_DYNAMICOBJECT_HPP_
