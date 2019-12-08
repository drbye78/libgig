/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2017-2019 Christian Schoenebeck                         *
 *                           <cuse@users.sourceforge.net>                  *
 *                                                                         *
 *   This library is part of libgig.                                       *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this library; if not, write to the Free Software           *
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston,                 *
 *   MA  02111-1307  USA                                                   *
 ***************************************************************************/

#ifndef LIBGIG_SERIALIZATION_H
#define LIBGIG_SERIALIZATION_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <typeinfo>
#include <string>
#include <vector>
#include <map>
#include <time.h>
#include <stdarg.h>

#ifndef __has_extension
# define __has_extension(x) 0
#endif

#ifndef HAS_BUILTIN_TYPE_TRAITS
# if __cplusplus >= 201103L
#  define HAS_BUILTIN_TYPE_TRAITS 1
# elif ( __has_extension(is_class) && __has_extension(is_enum) )
#  define HAS_BUILTIN_TYPE_TRAITS 1
# elif ( __GNUC__ > 4 || ( __GNUC__ == 4 && __GNUC_MINOR__ >= 3 ) )
#  define HAS_BUILTIN_TYPE_TRAITS 1
# elif _MSC_VER >= 1400 /* MS Visual C++ 8.0 (Visual Studio 2005) */
#  define HAS_BUILTIN_TYPE_TRAITS 1
# elif __INTEL_COMPILER >= 1100
#  define HAS_BUILTIN_TYPE_TRAITS 1
# else
#  define HAS_BUILTIN_TYPE_TRAITS 0
# endif
#endif

#if !HAS_BUILTIN_TYPE_TRAITS
# include <tr1/type_traits>
# define LIBGIG_IS_CLASS(type) std::tr1::__is_union_or_class<type>::value //NOTE: without compiler support we cannot distinguish union from class
#else
# define LIBGIG_IS_CLASS(type) __is_class(type)
#endif

/** @brief Serialization / deserialization framework.
 *
 * See class Archive as starting point for how to implement serialization and
 * deserialization with your application.
 *
 * The classes in this namespace allow to serialize and deserialize native
 * C++ objects in a portable, easy and flexible way. Serialization is a
 * technique that allows to transform the current state and data of native
 * (in this case C++) objects into a data stream (including all other objects
 * the "serialized" objects relate to); the data stream may then be sent over
 * "wire" (for example via network connection to another computer, which might
 * also have a different OS, CPU architecture, native memory word size and
 * endian type); and finally the data stream would be "deserialized" on that
 * receiver side, that is transformed again to modify all objects and data
 * structures on receiver side to resemble the objects' state and data as it
 * was originally on sender side.
 *
 * In contrast to many other already existing serialization frameworks, this
 * implementation has a strong focus on robustness regarding long-term changes
 * to the serialized C++ classes of the serialized objects. So even if sender
 * and receiver are using different versions of their serialized/deserialized
 * C++ classes, structures and data types (thus having different data structure
 * layout to a certain extent), this framework aims trying to automatically
 * adapt its serialization and deserialization process in that case so that
 * the deserialized objects on receiver side would still reflect the overall
 * expected states and overall data as intended by the sender. For being able to
 * do so, this framework stores all kind of additional information about each
 * serialized object and each data structure member (for example name of each
 * data structure member, but also the offset of each member within its
 * containing data structure, precise data types, and more).
 *
 * Like most other serialization frameworks, this frameworks does not require a
 * tree-structured layout of the serialized data structures. So it automatically
 * handles also cyclic dependencies between serialized data structures
 * correctly, without i.e. causing endless recursion or redundancy.
 *
 * Additionally this framework also allows partial deserialization. Which means
 * the receiver side may for example decide that it wants to restrict
 * deserialization so that it would only modify certain objects or certain
 * members by the deserialization process, leaving all other ones untouched.
 * So this partial deserialization technique for example allows to implement
 * flexible preset features for applications in a powerful and easy way.
 */
namespace Serialization {

    // just symbol prototyping
    class DataType;
    class Object;
    class Member;
    class Archive;
    class ObjectPool;
    class Exception;

    typedef std::string String;

    /** @brief Raw data stream of serialized C++ objects.
     *
     * This data type is used for the data stream as a result of serializing
     * your C++ objects with Archive::serialize(), and for native raw data
     * representation of individual serialized C/C++ objects, members and variables.
     *
     * @see Archive::rawData(), Object::rawData()
     */
    typedef std::vector<uint8_t> RawData;

    /** @brief Abstract identifier for serialized C++ objects.
     *
     * This data type is used for identifying serialized C++ objects and members
     * of your C++ objects. It is important to know that such an ID might not
     * necessarily be unique. For example the ID of one C++ object might often
     * be identical to the ID of the first member of that particular C++ object.
     * That's why there is additionally the concept of an UID in this framework.
     *
     * @see UID
     */
    typedef void* ID;

    /** @brief Version number data type.
     *
     * This data type is used for maintaining version number information of
     * your C++ class implementations.
     *
     * @see Archive::setVersion() and Archive::setMinVersion()
     */
    typedef uint32_t Version;

    /** @brief To which time zone a certain timing information relates to.
     *
     * The constants in this enum type are used to define to which precise time
     * zone a time stamp relates to.
     */
    enum time_base_t {
        LOCAL_TIME, ///< The time stamp relates to the machine's local time zone. Request a time stamp in local time if you want to present that time stamp to the end user.
        UTC_TIME ///< The time stamp relates to "Greenwhich Mean Time" zone, also known as "Coordinated Universal Time". Request time stamp with UTC if you want to compare that time stamp with other time stamps.
    };

    /** @brief Check whether data is a C/C++ @c enum type.
     *
     * Returns true if the supplied C++ variable or object is of a C/C++ @c enum
     * type.
     *
     * @param data - the variable or object whose data type shall be checked
     */
    template<typename T>
    bool IsEnum(const T& data) {
        #if !HAS_BUILTIN_TYPE_TRAITS
        return std::tr1::is_enum<T>::value;
        #else
        return __is_enum(T);
        #endif
    }

    /** @brief Check whether data is a C++ @c union type.
     *
     * Returns true if the supplied C++ variable or object is of a C/C++ @c union
     * type. Note that the result of this function is only reliable if the C++
     * compiler you are using has support for built-in type traits. If your C++
     * compiler does not have built-in type traits support, then this function
     * will simply return @c false on all your calls.
     *
     * @param data - the variable or object whose data type shall be checked
     */
    template<typename T>
    bool IsUnion(const T& data) {
        #if !HAS_BUILTIN_TYPE_TRAITS
        return false; // without compiler support we cannot distinguish union from class
        #else
        return __is_union(T);
        #endif
    }

    /** @brief Check whether data is a C/C++ @c struct or C++ @c class type.
     *
     * Returns true if the supplied C++ variable or object is of C/C++ @c struct
     * or C++ @c class type. Note that if you are using a C++ compiler which
     * does have built-in type traits support, then this function will also
     * return @c true on C/C++ @c union types.
     *
     * @param data - the variable or object whose data type shall be checked
     */
    template<typename T>
    bool IsClass(const T& data) {
        #if !HAS_BUILTIN_TYPE_TRAITS
        return std::tr1::__is_union_or_class<T>::value; // without compiler support we cannot distinguish union from class
        #else
        return __is_class(T);
        #endif
    }

    /*template<typename T>
    bool IsTrivial(T data) {
        return __is_trivial(T);
    }*/

    /*template<typename T>
    bool IsPOD(T data) {
        return __is_pod(T);
    }*/

    /** @brief Unique identifier referring to one specific native C++ object, member, fundamental variable, or any other native C++ data.
     *
     * Reflects a unique identifier for one specific serialized C++ data, i.e.
     * C++ class instance, C/C++ struct instance, member, primitive pointer,
     * fundamental variables, or any other native C/C++ data originally being
     * serialized.
     *
     * A unique identifier is composed of an id (an identifier which is not
     * necessarily unique) and a size. Since the underlying ID is derived from
     * the original C++ object's memory location, such an ID is not sufficient
     * to distinguish a particular C++ object from the first member of that C++
     * object, since both typically share the same memory address. So
     * additionally the memory size of the respective object or member is
     * bundled with UID objects to make them unique and distinguishable.
     */
    class UID {
    public:
        ID id; ///< Abstract non-unique ID of the object or member in question.
        size_t size; ///< Memory size of the object or member in question.

        bool isValid() const;
        operator bool() const { return isValid(); } ///< Same as calling isValid().
        //bool operator()() const { return isValid(); }
        bool operator==(const UID& other) const { return id == other.id && size == other.size; }
        bool operator!=(const UID& other) const { return id != other.id || size != other.size; }
        bool operator<(const UID& other) const { return id < other.id || (id == other.id && size < other.size); }
        bool operator>(const UID& other) const { return id > other.id || (id == other.id && size > other.size); }

        /** @brief Create an unique indentifier for a native C++ object/member/variable.
         *
         * Creates and returns an unique identifier for the passed native C++
         * object, object member or variable. For the same C++ object/member/variable
         * this function will always return the same UID. For all other ones,
         * this function is guaranteed to return a different UID.
         */
        template<typename T>
        static UID from(const T& obj) {
            return Resolver<T>::resolve(obj);
        }

    protected:
        // UID resolver for non-pointer types
        template<typename T>
        struct Resolver {
            static UID resolve(const T& obj) {
                const UID uid = { (ID) &obj, sizeof(obj) };
                return uid;
            }
        };

        // UID resolver for pointer types (of 1st degree)
        template<typename T>
        struct Resolver<T*> {
            static UID resolve(const T* const & obj) {
                const UID uid = { (ID) obj, sizeof(*obj) };
                return uid;
            }
        };
    };

    /**
     * Reflects an invalid UID and behaves similar to NULL as invalid value for
     * pointer types. All UID objects are first initialized with this value,
     * and it essentially an all zero object.
     */
    extern const UID NO_UID;

    /** @brief Chain of UIDs.
     *
     * This data type is used for native C++ pointers. The first member of the
     * UID chain is the unique identifier of the C++ pointer itself, then the
     * following UIDs are the respective objects or variables the pointer is
     * pointing to. The size (the amount of elements) of the UIDChain depends
     * solely on the degree of the pointer type. For example the following C/C++
     * pointer:
     * @code
     * int* pNumber;
     * @endcode
     * is an integer pointer of first degree. Such a pointer would have a
     * UIDChain with 2 members: the first element would be the UID of the
     * pointer itself, the second element of the chain would be the integer data
     * that pointer is pointing to. In the following example:
     * @code
     * bool*** pppSomeFlag;
     * @endcode
     * That boolean pointer would be of third degree, and thus its UIDChain
     * would have a size of 4 (elements).
     *
     * Accordingly a non pointer type like:
     * @code
     * float f;
     * @endcode
     * would yield in a UIDChain of size 1.
     *
     * Since however this serialization framework currently only supports
     * pointers of first degree yet, all UIDChains are currently either of
     * size 1 or 2, which might change in future though.
     */
    typedef std::vector<UID> UIDChain;

#if LIBGIG_SERIALIZATION_INTERNAL
    // prototyping of private internal friend functions
    static String _encodePrimitiveValue(const Object& obj);
    static DataType _popDataTypeBlob(const char*& p, const char* end);
    static Member _popMemberBlob(const char*& p, const char* end);
    static Object _popObjectBlob(const char*& p, const char* end);
    static void _popPrimitiveValue(const char*& p, const char* end, Object& obj);
    static String _primitiveObjectValueToString(const Object& obj);
    //  |
    template<typename T>
    static T _primitiveObjectValueToNumber(const Object& obj);
#endif // LIBGIG_SERIALIZATION_INTERNAL

    /** @brief Abstract reflection of a native C++ data type.
     *
     * Provides detailed information about a serialized C++ data type, whether
     * it is a fundamental C/C++ data type (like @c int, @c float, @c char,
     * etc.) or custom defined data types like a C++ @c class, C/C++ @c struct,
     * @c enum, as well as other features of the respective data type like its
     * native memory size and more.
     *
     * All informations provided by this class are retrieved from the
     * respective individual C++ objects, their members and other data when
     * they are serialized, and all those information are stored with the
     * serialized archive and its resulting data stream. Due to the availability
     * of these extensive data type information within serialized archives, this
     * framework is capable to use them in order to adapt its deserialization
     * process upon subsequent changes to your individual C++ classes.
     */
    class DataType {
    public:
        DataType();
        size_t size() const { return m_size; } ///< Returns native memory size of the respective C++ object or variable.
        bool isValid() const;
        bool isPointer() const;
        bool isClass() const;
        bool isPrimitive() const;
        bool isInteger() const;
        bool isReal() const;
        bool isBool() const;
        bool isEnum() const;
        bool isSigned() const;
        operator bool() const { return isValid(); } ///< Same as calling isValid().
        //bool operator()() const { return isValid(); }
        bool operator==(const DataType& other) const;
        bool operator!=(const DataType& other) const;
        bool operator<(const DataType& other) const;
        bool operator>(const DataType& other) const;
        String asLongDescr() const;
        String baseTypeName() const;
        String customTypeName(bool demangle = false) const;

        /** @brief Construct a DataType object for the given native C++ data.
         *
         * Use this function to create corresponding DataType objects for
         * native C/C++ objects, members and variables.
         *
         * @param data - native C/C++ object/member/variable a DataType object
         *               shall be created for
         * @returns corresponding DataType object for the supplied native C/C++
         *          object/member/variable
         */
        template<typename T>
        static DataType dataTypeOf(const T& data) {
            return Resolver<T>::resolve(data);
        }

    protected:
        DataType(bool isPointer, int size, String baseType, String customType = "");

        template<typename T, bool T_isPointer>
        struct ResolverBase {
            static DataType resolve(const T& data) {
                const std::type_info& type = typeid(data);
                const int sz = sizeof(data);

                // for primitive types we are using our own type names instead of
                // using std:::type_info::name(), because the precise output of the
                // latter may vary between compilers
                if (type == typeid(int8_t))   return DataType(T_isPointer, sz, "int8");
                if (type == typeid(uint8_t))  return DataType(T_isPointer, sz, "uint8");
                if (type == typeid(int16_t))  return DataType(T_isPointer, sz, "int16");
                if (type == typeid(uint16_t)) return DataType(T_isPointer, sz, "uint16");
                if (type == typeid(int32_t))  return DataType(T_isPointer, sz, "int32");
                if (type == typeid(uint32_t)) return DataType(T_isPointer, sz, "uint32");
                if (type == typeid(int64_t))  return DataType(T_isPointer, sz, "int64");
                if (type == typeid(uint64_t)) return DataType(T_isPointer, sz, "uint64");
                if (type == typeid(bool))     return DataType(T_isPointer, sz, "bool");
                if (type == typeid(float))    return DataType(T_isPointer, sz, "real32");
                if (type == typeid(double))   return DataType(T_isPointer, sz, "real64");

                if (IsEnum(data)) return DataType(T_isPointer, sz, "enum", rawCppTypeNameOf(data));
                if (IsUnion(data)) return DataType(T_isPointer, sz, "union", rawCppTypeNameOf(data));
                if (IsClass(data)) return DataType(T_isPointer, sz, "class", rawCppTypeNameOf(data));

                return DataType();
            }
        };

        // DataType resolver for non-pointer types
        template<typename T>
        struct Resolver : ResolverBase<T,false> {
            static DataType resolve(const T& data) {
                return ResolverBase<T,false>::resolve(data);
            }
        };

        // DataType resolver for pointer types (of 1st degree)
        template<typename T>
        struct Resolver<T*> : ResolverBase<T,true> {
            static DataType resolve(const T*& data) {
                return ResolverBase<T,true>::resolve(*data);
            }
        };

        template<typename T>
        static String rawCppTypeNameOf(const T& data) {
            #if defined _MSC_VER // Microsoft compiler ...
            String name = typeid(data).raw_name();
            #else // i.e. especially GCC and clang ...
            String name = typeid(data).name();
            #endif
            //while (!name.empty() && name[0] >= 0 && name[0] <= 9)
            //    name = name.substr(1);
            return name;
        }

    private:
        String m_baseTypeName;
        String m_customTypeName;
        int m_size;
        bool m_isPointer;

#if LIBGIG_SERIALIZATION_INTERNAL
        friend DataType _popDataTypeBlob(const char*& p, const char* end);
#endif
        friend class Archive;
    };

    /** @brief Abstract reflection of a native C++ class/struct's member variable.
     *
     * Provides detailed information about a specific C++ member variable of
     * serialized C++ object, like its C++ data type, offset of this member
     * within its containing data structure/class, its C++ member variable name
     * and more.
     *
     * Consider you defined the following user defined C/C++ @c struct type in
     * your application:
     * @code
     * struct Foo {
     *     int  a;
     *     bool b;
     *     double someValue;
     * };
     * @endcode
     * Then @c a, @c b and @c someValue are "members" of @c struct @c Foo for
     * instance. So that @c struct would have 3 members in the latter example.
     *
     * @see Object::members()
     */
    class Member {
    public:
        Member();
        UID uid() const;
        String name() const;
        size_t offset() const;
        const DataType& type() const;
        bool isValid() const;
        operator bool() const { return isValid(); } ///< Same as calling isValid().
        //bool operator()() const { return isValid(); }
        bool operator==(const Member& other) const;
        bool operator!=(const Member& other) const;
        bool operator<(const Member& other) const;
        bool operator>(const Member& other) const;

    protected:
        Member(String name, UID uid, size_t offset, DataType type);
        friend class Archive;

    private:
        UID m_uid;
        size_t m_offset;
        String m_name;
        DataType m_type;

#if LIBGIG_SERIALIZATION_INTERNAL
        friend Member _popMemberBlob(const char*& p, const char* end);
#endif
    };

    /** @brief Abstract reflection of some native serialized C/C++ data.
     *
     * When your native C++ objects are serialized, all native data is
     * translated and reflected by such an Object reflection. So each instance
     * of your serialized native C++ class objects become available as an
     * Object, but also each member variable of your C++ objects is translated
     * into an Object, and any other native C/C++ data. So essentially every
     * native data is turned into its own Object and accessible by this API.
     *
     * For each one of those Object reflections, this class provides detailed
     * information about their native origin. For example if an Object
     * represents a native C++ class instante, then it provides access to its
     * C++ class/struct name, to its C++ member variables, its native memory
     * size and much more.
     *
     * Even though this framework allows you to adjust abstract Object instances
     * to a certain extent, most of the methods of this Object class are
     * read-only though and the actual modifyable methods are made available
     * not as part of this Object class, but as part of the Archive class
     * instead. This design decision was made for performance and safety
     * reasons.
     *
     * @see Archive::setIntValue() as an example for modifying Object instances.
     */
    class Object {
    public:
        Object();
        Object(UIDChain uidChain, DataType type);

        UID uid(int index = 0) const;
        const UIDChain& uidChain() const;
        const DataType& type() const;
        const RawData& rawData() const;
        Version version() const;
        Version minVersion() const;
        bool isVersionCompatibleTo(const Object& other) const;
        std::vector<Member>& members();
        const std::vector<Member>& members() const;
        Member memberNamed(String name) const;
        Member memberByUID(const UID& uid) const;
        std::vector<Member> membersOfType(const DataType& type) const;
        int sequenceIndexOf(const Member& member) const;
        bool isValid() const;
        operator bool() const { return isValid(); } ///< Same as calling isValid().
        //bool operator()() const { return isValid(); }
        bool operator==(const Object& other) const;
        bool operator!=(const Object& other) const;
        bool operator<(const Object& other) const;
        bool operator>(const Object& other) const;

    protected:
        void remove(const Member& member);
        void setVersion(Version v);
        void setMinVersion(Version v);

    private:
        DataType m_type;
        UIDChain m_uid;
        Version m_version;
        Version m_minVersion;
        RawData m_data;
        std::vector<Member> m_members;

#if LIBGIG_SERIALIZATION_INTERNAL
        friend String _encodePrimitiveValue(const Object& obj);
        friend Object _popObjectBlob(const char*& p, const char* end);
        friend void _popPrimitiveValue(const char*& p, const char* end, Object& obj);
        friend String _primitiveObjectValueToString(const Object& obj);
        // |
        template<typename T>
        friend T _primitiveObjectValueToNumber(const Object& obj);
#endif // LIBGIG_SERIALIZATION_INTERNAL

        friend class Archive;
    };

    /** @brief Destination container for serialization, and source container for deserialization.
     *
     * This is the main class for implementing serialization and deserialization
     * with your C++ application. This framework does not require a a tree
     * structured layout of your C++ objects being serialized/deserialized, it
     * uses a concept of a "root" object though. So to start serialization
     * construct an empty Archive object and then instruct it to serialize your
     * C++ objects by pointing it to your "root" object:
     * @code
     * Archive a;
     * a.serialize(&myRootObject);
     * @endcode
     * Or if you prefer the look of operator based code:
     * @code
     * Archive a;
     * a << myRootObject;
     * @endcode
     * The Archive object will then serialize all members of the passed C++
     * object, and will recursively serialize all other C++ objects which it
     * contains or points to. So the root object is the starting point for the
     * overall serialization. After the serialize() method returned, you can
     * then access the serialized data stream by calling rawData() and send that
     * data stream over "wire", or store it on disk or whatever you may intend
     * to do with it.
     *
     * Then on receiver side likewise, you create a new Archive object, pass the
     * received data stream i.e. via constructor to the Archive object and call
     * deserialize() by pointing it to the root object on receiver side:
     * @code
     * Archive a(rawDataStream);
     * a.deserialize(&myRootObject);
     * @endcode
     * Or with operator instead:
     * @code
     * Archive a(rawDataStream);
     * a >> myRootObject;
     * @endcode
     * Now this framework automatically handles serialization and
     * deserialization of fundamental data types automatically for you (like
     * i.e. char, int, long int, float, double, etc.). However for your own
     * custom C++ classes and structs you must implement one method which
     * defines which members of your class should actually be serialized and
     * deserialized. That method to be added must have the following signature:
     * @code
     * void serialize(Serialization::Archive* archive);
     * @endcode
     * So let's say you have the following simple data structures:
     * @code
     * struct Foo {
     *     int a;
     *     bool b;
     *     double c;
     * };
     *
     * struct Bar {
     *     char  one;
     *     float two;
     *     Foo   foo1;
     *     Foo*  pFoo2;
     *     Foo*  pFoo3DontTouchMe; // shall not be serialized/deserialized
     * };
     * @endcode
     * So in order to be able to serialize and deserialize objects of those two
     * structures you would first add the mentioned method to each struct
     * definition (i.e. in your header file):
     * @code
     * struct Foo {
     *     int a;
     *     bool b;
     *     double c;
     *
     *     void serialize(Serialization::Archive* archive);
     * };
     *
     * struct Bar {
     *     char  one;
     *     float two;
     *     Foo   foo1;
     *     Foo*  pFoo2;
     *     Foo*  pFoo3DontTouchMe; // shall not be serialized/deserialized
     *
     *     void serialize(Serialization::Archive* archive);
     * };
     * @endcode
     * And then you would implement those two new methods like this (i.e. in
     * your .cpp file):
     * @code
     * #define SRLZ(member) \
     *   archive->serializeMember(*this, member, #member);
     *
     * void Foo::serialize(Serialization::Archive* archive) {
     *     SRLZ(a);
     *     SRLZ(b);
     *     SRLZ(c);
     * }
     *
     * void Bar::serialize(Serialization::Archive* archive) {
     *     SRLZ(one);
     *     SRLZ(two);
     *     SRLZ(foo1);
     *     SRLZ(pFoo2);
     *     // leaving out pFoo3DontTouchMe here
     * }
     * @endcode
     * Now when you serialize such a Bar object, this framework will also
     * automatically serialize the respective Foo object(s) accordingly, also
     * for the pFoo2 pointer for instance (as long as it is not a NULL pointer
     * that is).
     *
     * Note that there is only one method that you need to implement. So the
     * respective serialize() method implementation of your classes/structs are
     * both called for serialization, as well as for deserialization!
     *
     * In case you need to enforce backward incompatiblity for one of your C++
     * classes, you can do so by setting a version and minimum version for your
     * class (see @c setVersion() and @c setMinVersion() for details).
     */
    class Archive {
    public:
        Archive();
        Archive(const RawData& data);
        Archive(const uint8_t* data, size_t size);
        virtual ~Archive();

        /** @brief Initiate serialization.
         *
         * Initiates serialization of all native C++ objects, which means
         * capturing and storing the current data of all your C++ objects as
         * content of this Archive.
         *
         * This framework has a concept of a "root" object which you must pass
         * to this method. The root object is the starting point for
         * serialization of your C++ objects. The framework will then
         * recursively serialize all members of that C++ object an continue to
         * serialize all other C++ objects that it might contain or point to.
         *
         * After this method returned, you might traverse all serialized objects
         * by walking them starting from the rootObject(). You might then modify
         * that abstract reflection of your C++ objects and finally you might
         * call rawData() to get an encoded raw data stream which you might use
         * for sending it "over wire" to somewhere where it is going to be
         * deserialized later on.
         *
         * Note that whenever you call this method, the previous content of this
         * Archive will first be cleared.
         *
         * @param obj - native C++ root object where serialization shall start
         * @see Archive::operator<<()
         */
        template<typename T>
        void serialize(const T* obj) {
            m_operation = OPERATION_SERIALIZE;
            m_allObjects.clear();
            m_rawData.clear();
            m_root = UID::from(obj);
            const_cast<T*>(obj)->serialize(this);
            encode();
            m_operation = OPERATION_NONE;
        }

        /** @brief Initiate deserialization.
         *
         * Initiates deserialization of all native C++ objects, which means all
         * your C++ objects will be restored with the values contained in this
         * Archive. So that also means calling deserialize() only makes sense if
         * this a non-empty Archive, which i.e. is the case if you either called
         * serialize() with this Archive object before or if you passed a
         * previously serialized raw data stream to the constructor of this
         * Archive object.
         * 
         * This framework has a concept of a "root" object which you must pass
         * to this method. The root object is the starting point for
         * deserialization of your C++ objects. The framework will then
         * recursively deserialize all members of that C++ object an continue to
         * deserialize all other C++ objects that it might contain or point to,
         * according to the values stored in this Archive.
         *
         * @param obj - native C++ root object where deserialization shall start
         * @see Archive::operator>>()
         *
         * @throws Exception if the data stored in this Archive cannot be
         *         restored to the C++ objects passed to this method, i.e.
         *         because of version or type incompatibilities.
         */
        template<typename T>
        void deserialize(T* obj) {
            Archive a;
            m_operation = OPERATION_DESERIALIZE;
            obj->serialize(&a);
            a.m_root = UID::from(obj);
            Syncer s(a, *this);
            m_operation = OPERATION_NONE;
        }

        /** @brief Initiate serialization of your C++ objects.
         *
         * Same as calling @c serialize(), this is just meant if you prefer
         * to use operator based code instead, which you might find to be more
         * intuitive.
         *
         * Example:
         * @code
         * Archive a;
         * a << myRootObject;
         * @endcode
         *
         * @see Archive::serialize() for more details.
         */
        template<typename T>
        void operator<<(const T& obj) {
            serialize(&obj);
        }

        /** @brief Initiate deserialization of your C++ objects.
         *
         * Same as calling @c deserialize(), this is just meant if you prefer
         * to use operator based code instead, which you might find to be more
         * intuitive.
         *
         * Example:
         * @code
         * Archive a(rawDataStream);
         * a >> myRootObject;
         * @endcode
         *
         * @throws Exception if the data stored in this Archive cannot be
         *         restored to the C++ objects passed to this method, i.e.
         *         because of version or type incompatibilities.
         *
         * @see Archive::deserialize() for more details.
         */
        template<typename T>
        void operator>>(T& obj) {
            deserialize(&obj);
        }

        const RawData& rawData();
        virtual String rawDataFormat() const;

        /** @brief Serialize a native C/C++ member variable.
         *
         * This method is usually called by the serialize() method
         * implementation of your C/C++ structs and classes, for each of the
         * member variables that shall be serialized and deserialized
         * automatically with this framework. It is recommend that you are not
         * using this method name directly, but rather define a short hand C
         * macro in your .cpp file like:
         * @code
         * #define SRLZ(member) \
         *   archive->serializeMember(*this, member, #member);
         *
         * void Foo::serialize(Serialization::Archive* archive) {
         *     SRLZ(a);
         *     SRLZ(b);
         *     SRLZ(c);
         * }
         * @endcode
         * As you can see, using such a macro makes your code more readable and
         * less error prone.
         *
         * It is completely up to you to decide which ones of your member
         * variables shall automatically be serialized and deserialized with
         * this framework. Only those member variables which are registered by
         * calling this method will be serialized and deserialized. It does not
         * really matter in which order you register your individiual member
         * variables by calling this method, but the sequence is actually stored
         * as meta information with the resulting archive and the resulting raw
         * data stream. That meta information might then be used by this
         * framework to automatically correct and adapt deserializing that
         * archive later on for a future (or older) and potentially heavily
         * modified version of your software. So it is recommended, even though
         * also not required, that you may retain the sequence of your
         * serializeMember() calls for your individual C++ classes' members over
         * all your software versions, to retain backward compatibility of older
         * archives as much as possible.
         *
         * @param nativeObject - native C++ object to be registered for
         *                       serialization / deserialization
         * @param nativeMember - native C++ member variable of @a nativeObject
         *                       to be registered for serialization /
         *                       deserialization
         * @param memberName - name of @a nativeMember to be stored with this
         *                     archive
         */
        template<typename T_classType, typename T_memberType>
        void serializeMember(const T_classType& nativeObject, const T_memberType& nativeMember, const char* memberName) {
            const size_t offset =
                ((const uint8_t*)(const void*)&nativeMember) -
                ((const uint8_t*)(const void*)&nativeObject);
            const UIDChain uids = UIDChainResolver<T_memberType>(nativeMember);
            const DataType type = DataType::dataTypeOf(nativeMember);
            const Member member(memberName, uids[0], offset, type);
            const UID parentUID = UID::from(nativeObject);
            Object& parent = m_allObjects[parentUID];
            if (!parent) {
                const UIDChain uids = UIDChainResolver<T_classType>(nativeObject);
                const DataType type = DataType::dataTypeOf(nativeObject);
                parent = Object(uids, type);
            }
            parent.members().push_back(member);
            const Object obj(uids, type);
            const bool bExistsAlready = m_allObjects.count(uids[0]);
            const bool isValidObject = obj;
            const bool bExistingObjectIsInvalid = !m_allObjects[uids[0]];
            if (!bExistsAlready || (bExistingObjectIsInvalid && isValidObject)) {
                m_allObjects[uids[0]] = obj;
                // recurse serialization for all members of this member
                // (only for struct/class types, noop for primitive types)
                SerializationRecursion<T_memberType>::serializeObject(this, nativeMember);
            }
        }

        /** @brief Set current version number for your C++ class.
         *
         * By calling this method you can define a version number for your
         * current C++ class (that is a version for its current data structure
         * layout and method implementations) that is going to be stored along
         * with the serialized archive. Only call this method if you really want
         * to constrain compatibility of your C++ class.
         *
         * Along with calling @c setMinVersion() this provides a way for you
         * to constrain backward compatibility regarding serialization and
         * deserialization of your C++ class which the Archive class will obey
         * to. If required, then typically you might do so in your
         * @c serialize() method implementation like:
         * @code
         * #define SRLZ(member) \
         *   archive->serializeMember(*this, member, #member);
         *
         * void Foo::serialize(Serialization::Archive* archive) {
         *     // when serializing: the current version of this class that is
         *     // going to be stored with the serialized archive
         *     archive->setVersion(*this, 6);
         *     // when deserializing: the minimum version this C++ class is
         *     // compatible with
         *     archive->setMinVersion(*this, 3);
         *     // actual data mebers to serialize / deserialize
         *     SRLZ(a);
         *     SRLZ(b);
         *     SRLZ(c);
         * }
         * @endcode
         * In this example above, the C++ class "Foo" would be serialized along
         * with the version number @c 6 and minimum version @c 3 as additional
         * meta information in the resulting archive (and its raw data stream
         * respectively).
         *
         * When deserializing archives with the example C++ class code above,
         * the Archive object would check whether your originally serialized
         * C++ "Foo" object had at least version number @c 3, if not the
         * deserialization process would automatically be stopped with a
         * @c Serialization::Exception, claiming that the classes are version
         * incompatible.
         *
         * But also consider the other way around: you might have serialized
         * your latest version of your C++ class, and might deserialize that
         * archive with an older version of your C++ class. In that case it will
         * likewise be checked whether the version of that old C++ class is at
         * least as high as the minimum version set with the already seralized
         * bleeding edge C++ class.
         *
         * Since this Serialization / deserialization framework is designed to
         * be robust on changes to your C++ classes and aims trying to
         * deserialize all your C++ objects correctly even if your C++ classes
         * have seen substantial software changes in the meantime; you might
         * sometimes see it as necessary to constrain backward compatibility
         * this way. Because obviously there are certain things this framework
         * can cope with, like for example that you renamed a data member while
         * keeping the layout consistent, or that you have added new members to
         * your C++ class or simply changed the order of your members in your
         * C++ class. But what this framework cannot detect is for example if
         * you changed the semantics of the values stored with your members, or
         * even substantially changed the algorithms in your class methods such
         * that they would not handle the data of your C++ members in the same
         * and correct way anymore.
         *
         * @param nativeObject - your C++ object you want to set a version for
         * @param v - the version number to set for your C++ class (by default,
         *            that is if you do not explicitly call this method, then
         *            your C++ object will be stored with version number @c 0 ).
         */
        template<typename T_classType>
        void setVersion(const T_classType& nativeObject, Version v) {
            const UID uid = UID::from(nativeObject);
            Object& obj = m_allObjects[uid];
            if (!obj) {
                const UIDChain uids = UIDChainResolver<T_classType>(nativeObject);
                const DataType type = DataType::dataTypeOf(nativeObject);
                obj = Object(uids, type);
            }
            setVersion(obj, v);
        }

        /** @brief Set a minimum version number for your C++ class.
         *
         * Call this method to define a minimum version that your current C++
         * class implementation would be compatible with when it comes to 
         * deserialization of an archive containing an object of your C++ class.
         * Like the version information, the minimum version will also be stored
         * for objects of your C++ class with the resulting archive (and its
         * resulting raw data stream respectively).
         *
         * When you start to constrain version compatibility of your C++ class
         * you usually start by using 1 as version and 1 as minimum version.
         * So it is eligible to set the same number to both version and minimum
         * version. However you must @b not set a minimum version higher than
         * version. Doing so would not raise an exception, but the resulting
         * behavior would be undefined.
         *
         * It is not relevant whether you first set version and then minimum
         * version or vice versa. It is also not relevant when exactly you set
         * those two numbers, even though usually you would set both in your
         * serialize() method implementation.
         *
         * @see @c setVersion() for more details about this overall topic.
         *
         * @param nativeObject - your C++ object you want to set a version for
         * @param v - the minimum version you want to define for your C++ class
         *            (by default, that is if you do not explicitly call this
         *            method, then a minium version of @c 0 is assumed for your
         *            C++ class instead).
         */
        template<typename T_classType>
        void setMinVersion(const T_classType& nativeObject, Version v) {
            const UID uid = UID::from(nativeObject);
            Object& obj = m_allObjects[uid];
            if (!obj) {
                const UIDChain uids = UIDChainResolver<T_classType>(nativeObject);
                const DataType type = DataType::dataTypeOf(nativeObject);
                obj = Object(uids, type);
            }
            setMinVersion(obj, v);
        }

        virtual void decode(const RawData& data);
        virtual void decode(const uint8_t* data, size_t size);
        void clear();
        bool isModified() const;
        void removeMember(Object& parent, const Member& member);
        void remove(const Object& obj);
        Object& rootObject();
        Object& objectByUID(const UID& uid);
        void setAutoValue(Object& object, String value);
        void setIntValue(Object& object, int64_t value);
        void setRealValue(Object& object, double value);
        void setBoolValue(Object& object, bool value);
        void setEnumValue(Object& object, uint64_t value);
        String valueAsString(const Object& object);
        int64_t valueAsInt(const Object& object);
        double valueAsReal(const Object& object);
        bool valueAsBool(const Object& object);
        void setVersion(Object& object, Version v);
        void setMinVersion(Object& object, Version v);
        String name() const;
        void setName(String name);
        String comment() const;
        void setComment(String comment);
        time_t timeStampCreated() const;
        time_t timeStampModified() const;
        tm dateTimeCreated(time_base_t base = LOCAL_TIME) const;
        tm dateTimeModified(time_base_t base = LOCAL_TIME) const;

    protected:
        // UID resolver for non-pointer types
        template<typename T>
        class UIDChainResolver {
        public:
            UIDChainResolver(const T& data) {
                m_uid.push_back(UID::from(data));
            }

            operator UIDChain() const { return m_uid; }
            UIDChain operator()() const { return m_uid; }
        private:
            UIDChain m_uid;
        };

        // UID resolver for pointer types (of 1st degree)
        template<typename T>
        class UIDChainResolver<T*> {
        public:
            UIDChainResolver(const T*& data) {
                const UID uids[2] = {
                    { &data, sizeof(data) },
                    { data, sizeof(*data) }
                };
                m_uid.push_back(uids[0]);
                m_uid.push_back(uids[1]);
            }

            operator UIDChain() const { return m_uid; }
            UIDChain operator()() const { return m_uid; }
        private:
            UIDChain m_uid;
        };

        // SerializationRecursion for non-pointer class/struct types.
        template<typename T, bool T_isRecursive>
        struct SerializationRecursionImpl {
            static void serializeObject(Archive* archive, const T& obj) {
                const_cast<T&>(obj).serialize(archive);
            }
        };

        // SerializationRecursion for pointers (of 1st degree) to class/structs.
        template<typename T, bool T_isRecursive>
        struct SerializationRecursionImpl<T*,T_isRecursive> {
            static void serializeObject(Archive* archive, const T*& obj) {
                if (!obj) return;
                const_cast<T*&>(obj)->serialize(archive);
            }
        };

        // NOOP SerializationRecursion for primitive types.
        template<typename T>
        struct SerializationRecursionImpl<T,false> {
            static void serializeObject(Archive* archive, const T& obj) {}
        };

        // NOOP SerializationRecursion for pointers (of 1st degree) to primitive types.
        template<typename T>
        struct SerializationRecursionImpl<T*,false> {
            static void serializeObject(Archive* archive, const T*& obj) {}
        };

        // Automatically handles recursion for class/struct types, while ignoring all primitive types.
        template<typename T>
        struct SerializationRecursion : SerializationRecursionImpl<T, LIBGIG_IS_CLASS(T)> {
        };

        class ObjectPool : public std::map<UID,Object> {
        public:
            // prevent passing obvious invalid UID values from creating a new pair entry
            Object& operator[](const UID& k) {
                static Object invalid;
                if (!k.isValid()) {
                    invalid = Object();
                    return invalid;
                }
                return std::map<UID,Object>::operator[](k);
            }
        };

        friend String _encode(const ObjectPool& objects);

    private:
        String _encodeRootBlob();
        void _popRootBlob(const char*& p, const char* end);
        void _popObjectsBlob(const char*& p, const char* end);

    protected:
        class Syncer {
        public:
            Syncer(Archive& dst, Archive& src);
        protected:
            void syncObject(const Object& dst, const Object& src);
            void syncPrimitive(const Object& dst, const Object& src);
            void syncPointer(const Object& dst, const Object& src);
            void syncMember(const Member& dstMember, const Member& srcMember);
            static Member dstMemberMatching(const Object& dstObj, const Object& srcObj, const Member& srcMember);
        private:
            Archive& m_dst;
            Archive& m_src;
        };

        enum operation_t {
            OPERATION_NONE,
            OPERATION_SERIALIZE,
            OPERATION_DESERIALIZE
        };

        virtual void encode();

        ObjectPool m_allObjects;
        operation_t m_operation;
        UID m_root;
        RawData m_rawData;
        bool m_isModified;
        String m_name;
        String m_comment;
        time_t m_timeCreated;
        time_t m_timeModified;
    };

    /**
     * Will be thrown whenever an error occurs during an serialization or
     * deserialization process.
     */
    class Exception {
        public:
            String Message;

            Exception(String format, ...);
            Exception(String format, va_list arg);
            void PrintMessage();
            virtual ~Exception() {}

        protected:
            Exception();
            static String assemble(String format, va_list arg);
    };

} // namespace Serialization

#endif // LIBGIG_SERIALIZATION_H
