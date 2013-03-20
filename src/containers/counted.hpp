// Copyright 2010-2013 RethinkDB, all rights reserved.
#ifndef CONTAINERS_COUNTED_HPP_
#define CONTAINERS_COUNTED_HPP_

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "errors.hpp"

// Yes, this is a clone of boost::counted_t.  This will probably
// not be the case in the future.

// Now it supports .unique(), and in order to use it, your type needs
// to provide an counted_t_use_count implementation.

template <class T>
class counted_t {
public:
    template <class U>
    friend class counted_t;

    counted_t() : p_(NULL) { }
    explicit counted_t(T *p) : p_(p) {
        if (p_) { counted_t_add_ref(p_); }
    }

    counted_t(const counted_t &copyee) : p_(copyee.p_) {
        if (p_) { counted_t_add_ref(p_); }
    }

    // TODO: Add noexcept on versions of compilers that support it.  noexcept is
    // good to have because types like std::vectors use it to see whether to
    // call the copy constructor or move constructor.
    counted_t(counted_t &&movee) : p_(movee.p_) {
        movee.p_ = NULL;
    }

    // TODO: Add noexcept on versions of compilers that support it.
    template <class U>
    counted_t(const counted_t<U> &&movee) : p_(movee.p_) {
        movee.p_ = NULL;
    }

    ~counted_t() {
        if (p_) { counted_t_release(p_); }
    }

    void swap(counted_t &other) {
        T *tmp = p_;
        p_ = other.p_;
        other.p_ = tmp;
    }

    // TODO: Add noexcept on versions of compilers that support it.
    counted_t &operator=(counted_t &&other) {
        counted_t tmp(std::move(other));
        swap(tmp);
        return *this;
    }

    // TODO: Add noexcept on versions of compilers that support it.
    template <class U>
    counted_t &operator=(counted_t<U> &&other) {
        counted_t tmp(std::move(other));
        swap(tmp);
        return *this;
    }

    counted_t &operator=(const counted_t &other) {
        counted_t tmp(other);
        swap(tmp);
        return *this;
    }

    void reset() {
        counted_t tmp;
        swap(tmp);
    }

    void reset(T *other) {
        counted_t tmp(other);
        swap(tmp);  // NOLINT
    }

    T *operator->() const {
        return p_;
    }

    T &operator*() const {
        return *p_;
    }

    T *get() const {
        return p_;
    }

    bool has() const {
        return p_ != NULL;
    }

    bool unique() const {
        return counted_t_use_count(p_) == 1;
    }

    class hidden_t {
        hidden_t();
    };
    typedef void booleanesque_t(hidden_t);

    operator booleanesque_t*() const {
        return p_ ? &counted_t<T>::dummy_method : 0;
    }

private:
    static void dummy_method(hidden_t) { }

    T *p_;
};

template <class T, class... Args>
counted_t<T> make_counted(Args... args) {
    return counted_t<T>(new T(args...));
}

template <class> class single_threaded_shared_mixin_t;

template <class T>
inline void counted_t_add_ref(single_threaded_shared_mixin_t<T> *p);
template <class T>
inline void counted_t_release(single_threaded_shared_mixin_t<T> *p);
template <class T>
inline intptr_t counted_t_use_count(const single_threaded_shared_mixin_t<T> *p);

template <class T>
class single_threaded_shared_mixin_t {
public:
    single_threaded_shared_mixin_t() : refcount_(0) { }

protected:
    ~single_threaded_shared_mixin_t() { }

private:
    friend void counted_t_add_ref<T>(single_threaded_shared_mixin_t<T> *p);
    friend void counted_t_release<T>(single_threaded_shared_mixin_t<T> *p);
    friend intptr_t counted_t_use_count<T>(const single_threaded_shared_mixin_t<T> *p);

    mutable intptr_t refcount_;
    DISABLE_COPYING(single_threaded_shared_mixin_t);
};

template <class T>
inline void counted_t_add_ref(const single_threaded_shared_mixin_t<T> *p) {
    p->refcount_ += 1;
    rassert(p->refcount_ > 0);
}

template <class T>
inline void counted_t_release(const single_threaded_shared_mixin_t<T> *p) {
    rassert(p->refcount_ > 0);
    --p->refcount_;
    if (p->refcount_ == 0) {
        delete static_cast<T *>(p);
    }
}

template <class T>
inline intptr_t counted_t_use_count(const single_threaded_shared_mixin_t<T> *p) {
    return p->refcount_;
}






template <class> class slow_shared_mixin_t;

template <class T>
inline void counted_t_add_ref(slow_shared_mixin_t<T> *p);
template <class T>
inline void counted_t_release(slow_shared_mixin_t<T> *p);
template <class T>
inline intptr_t counted_t_use_count(const slow_shared_mixin_t<T> *p);

template <class T>
class slow_shared_mixin_t {
public:
    slow_shared_mixin_t() : refcount_(0) { }

protected:
    ~slow_shared_mixin_t() { }

private:
    friend void counted_t_add_ref<T>(slow_shared_mixin_t<T> *p);
    friend void counted_t_release<T>(slow_shared_mixin_t<T> *p);
    friend intptr_t counted_t_use_count<T>(const slow_shared_mixin_t<T> *p);

    intptr_t refcount_;
    DISABLE_COPYING(slow_shared_mixin_t);
};

template <class T>
inline void counted_t_add_ref(slow_shared_mixin_t<T> *p) {
    DEBUG_VAR intptr_t res = __sync_add_and_fetch(&p->refcount_, 1);
    rassert(res > 0);
}

template <class T>
inline void counted_t_release(slow_shared_mixin_t<T> *p) {
    intptr_t res = __sync_sub_and_fetch(&p->refcount_, 1);
    rassert(res >= 0);
    if (res == 0) {
        delete static_cast<T *>(p);
    }
}

template <class T>
inline intptr_t counted_t_use_count(const slow_shared_mixin_t<T> *p) {
    // Finally a practical use for volatile.
    intptr_t tmp = static_cast<const volatile intptr_t&>(p->refcount_);
    rassert(tmp > 0);
    return tmp;
}


#endif  // CONTAINERS_COUNTED_HPP_