#pragma once
/*
**  Copyright (C) 2012 Aldebaran Robotics
**  See COPYING for the license
*/

#ifndef _QITYPE_SIGNATURE_HPP_
#define _QITYPE_SIGNATURE_HPP_

#include <qitype/api.hpp>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <sstream>
#include <boost/shared_ptr.hpp>

#ifdef _MSC_VER
#  pragma warning( push )
#  pragma warning( disable: 4251 )
#endif

namespace qi {

  QITYPE_API std::vector<std::string> signatureSplit(const std::string &fullSignature);

  class SignaturePrivate;

  /* Represent the serialisation signature of a Type.
  * pseudo-grammar:
  * root: element
  * element: signature annotation.opt
  * sinature:
  *   | primitive  // in bcCwWiIlLfdsmro, see Type
  *   | [element]
  *   | {elementelement}
  *   | (elementsequence)
  * elementsequence: a list of 1 or more elements
  * annotation.opt: empty or <annotation>
  * annotation: may contain arbitrary content except \0
      and must balance all (), {}, [] and <> within
  */
  class QITYPE_API Signature {
  public:

  public:
    Signature(const char *signature = 0);
    Signature(const std::string &signature);

    bool isValid() const;

    // Number of elements in the signature.
    unsigned int size() const;

    class iterator;
    iterator begin() const;
    iterator end() const;

    //TODO use the type than "network type"
    enum Type {
      // Used only for empty containers when Dynamic resolution is used.
      Type_None     = '_',
      Type_Bool     = 'b',

      Type_Int8     = 'c',
      Type_UInt8    = 'C',

      Type_Void     = 'v',

      Type_Int16    = 'w',
      Type_UInt16   = 'W',

      Type_Int32    = 'i',
      Type_UInt32   = 'I',

      Type_Int64    = 'l',
      Type_UInt64   = 'L',

      Type_Float    = 'f',
      Type_Double   = 'd',

      Type_String   = 's',
      Type_List     = '[',
      Type_List_End = ']',

      Type_Map      = '{',
      Type_Map_End  = '}',

      Type_Tuple    = '(',
      Type_Tuple_End= ')',

      Type_Dynamic  = 'm',

      Type_Raw      = 'r',

      //This type should not be used, it's will be removed when we get ride of legacy void *.
      Type_Pointer  = '*',

      Type_Object   = 'o',
      Type_Unknown  = 'X'
    };

    class QITYPE_API iterator {
    public:
      typedef std::string               value_type;
      typedef std::forward_iterator_tag iterator_category;
      typedef ptrdiff_t                 difference_type;
      typedef std::string*              pointer;
      typedef std::string&              reference;
      iterator() : _current(0), _end(0) {}
      iterator          &operator++();
      iterator          &operator++(int);
      inline bool        operator!=(const iterator &rhs) const { return _current != rhs._current; }
      inline bool        operator==(const iterator &rhs) const { return _current == rhs._current; };
      inline std::string operator*() const                     { return signature(); }
      inline std::string operator->() const                    { return signature(); }

      // accesors
      Type        type()const;
      std::string signature()const;
      std::string annotation()const;
      bool        isValid()const;

      bool        hasChildren()const;
      Signature   children()const;

    protected:
      iterator(const char *begin, const char *end) : _current(begin), _end(end) {}
      const char *_current;
      const char *_end;
      friend class qi::Signature;
    };



    std::string toSTLSignature(bool constify = false) const;
    std::string toQtSignature(bool constify = false) const;
    std::string toString() const;

    /** Tell if arguments with this signature can be converted to \p b.
     * @return 0 if conversion is impossible, or a score in ]0,1] indicating
     * the amount of type mismatch (the closer signatures are the bigger)
     */
    float isConvertibleTo(const Signature& b) const;

    static Signature fromType(Type t);
  protected:
    // C4251
    boost::shared_ptr<SignaturePrivate> _p;
  };
}

#ifdef _MSC_VER
#  pragma warning( pop )
#endif

#endif  // _QITYPE_SIGNATURE_HPP_
