/*
    Copyright (c) 2009-2011 250bpm s.r.o.
    Copyright (c) 2011 Botond Ballo
    Copyright (c) 2007-2009 iMatix Corporation

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal in the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#ifndef __XS_HPP_INCLUDED__
#define __XS_HPP_INCLUDED__

#include <xs/xs.h>

#include <cassert>
#include <cstring>
#include <algorithm>
#include <exception>

//  Detect whether the compiler supports C++11 rvalue references.
#if (defined(__GNUC__) && (__GNUC__ > 4 || \
      (__GNUC__ == 4 && __GNUC_MINOR__ > 2)) && \
      defined(__GXX_EXPERIMENTAL_CXX0X__))
    #define XS_HAS_RVALUE_REFS
#endif
#if (defined(__clang__))
    #if __has_feature(cxx_rvalue_references)
        #define XS_HAS_RVALUE_REFS
    #endif
#endif
#if (defined(_MSC_VER) && (_MSC_VER >= 1600))
    #define XS_HAS_RVALUE_REFS
#endif

// In order to prevent unused variable warnings when building in non-debug
// mode use this macro to make assertions.
#ifndef NDEBUG
#   define XS_ASSERT(expression) assert(expression)
#else
#   define XS_ASSERT(expression) (expression)
#endif

namespace xs
{

    typedef xs_free_fn free_fn;
    typedef xs_pollitem_t pollitem_t;

    class error_t : public std::exception
    {
    public:

        error_t () : errnum (xs_errno ()) {}

        virtual const char *what () const throw ()
        {
            return xs_strerror (errnum);
        }

        int num () const
        {
            return errnum;
        }

    private:

        int errnum;
    };

    inline int poll (xs_pollitem_t *items_, int nitems_, long timeout_ = -1)
    {
        int rc = xs_poll (items_, nitems_, timeout_);
        if (rc < 0)
            throw error_t ();
        return rc;
    }

    inline void version (int *major_, int *minor_, int *patch_)
    {
        xs_version (major_, minor_, patch_);
    }

    class message_t
    {
        friend class socket_t;

    public:

        inline message_t ()
        {
            int rc = xs_msg_init (&msg);
            if (rc != 0)
                throw error_t ();
        }

        inline message_t (size_t size_)
        {
            int rc = xs_msg_init_size (&msg, size_);
            if (rc != 0)
                throw error_t ();
        }

        inline message_t (void *data_, size_t size_, free_fn *ffn_,
            void *hint_ = NULL)
        {
            int rc = xs_msg_init_data (&msg, data_, size_, ffn_, hint_);
            if (rc != 0)
                throw error_t ();
        }

        inline ~message_t ()
        {
            int rc = xs_msg_close (&msg);
            XS_ASSERT (rc == 0);
        }

        inline void rebuild ()
        {
            int rc = xs_msg_close (&msg);
            if (rc != 0)
                throw error_t ();
            rc = xs_msg_init (&msg);
            if (rc != 0)
                throw error_t ();
        }

        inline void rebuild (size_t size_)
        {
            int rc = xs_msg_close (&msg);
            if (rc != 0)
                throw error_t ();
            rc = xs_msg_init_size (&msg, size_);
            if (rc != 0)
                throw error_t ();
        }

        inline void rebuild (void *data_, size_t size_, free_fn *ffn_,
            void *hint_ = NULL)
        {
            int rc = xs_msg_close (&msg);
            if (rc != 0)
                throw error_t ();
            rc = xs_msg_init_data (&msg, data_, size_, ffn_, hint_);
            if (rc != 0)
                throw error_t ();
        }

        inline void move (message_t *msg_)
        {
            int rc = xs_msg_move (&msg, &(msg_->msg));
            if (rc != 0)
                throw error_t ();
        }

        inline void copy (message_t *msg_)
        {
            int rc = xs_msg_copy (&msg, &(msg_->msg));
            if (rc != 0)
                throw error_t ();
        }

        inline void *data ()
        {
            return xs_msg_data (&msg);
        }

        inline size_t size ()
        {
            return xs_msg_size (&msg);
        }

        inline void getmsgopt (int option_, void *optval_,
            size_t *optvallen_)
        {
            int rc = xs_getmsgopt (&msg, option_, optval_, optvallen_);
            if (rc != 0)
                throw error_t ();
        }

    private:

        //  The underlying message
        xs_msg_t msg;

        //  Disable implicit message copying, so that users won't use shared
        //  messages (less efficient) without being aware of the fact.
        message_t (const message_t&);
        void operator = (const message_t&);
    };

    class context_t
    {
        friend class socket_t;

    public:

        inline context_t ()
        {
            ptr = xs_init ();
            if (ptr == NULL)
                throw error_t ();
        }

#ifdef XS_HAS_RVALUE_REFS
        inline context_t (context_t &&rhs) : ptr (rhs.ptr)
        {
            rhs.ptr = NULL;
        }
        inline context_t &operator = (context_t &&rhs)
        {
            std::swap (ptr, rhs.ptr);
            return *this;
        }
#endif

        inline ~context_t ()
        {
            if (ptr == NULL)
                return;
            int rc = xs_term (ptr);
            XS_ASSERT (rc == 0);
        }

        inline void setctxopt (int option_, const void *optval_,
            size_t optvallen_)
        {
            int rc = xs_setctxopt (ptr, option_, optval_, optvallen_);
            if (rc != 0)
                throw error_t ();
        }

        //  Be careful with this, it's probably only useful for
        //  using the C api together with an existing C++ api.
        //  Normally you should never need to use this.
        inline operator void* ()
        {
            return ptr;
        }

    private:

        void *ptr;

        context_t (const context_t&);
        void operator = (const context_t&);
    };

    class socket_t
    {
    public:

        inline socket_t (context_t &context_, int type_)
        {
            ptr = xs_socket (context_.ptr, type_);
            if (ptr == NULL)
                throw error_t ();
        }

#ifdef XS_HAS_RVALUE_REFS
        inline socket_t(socket_t&& rhs) : ptr(rhs.ptr)
        {
            rhs.ptr = NULL;
        }
        inline socket_t& operator=(socket_t&& rhs)
        {
            std::swap(ptr, rhs.ptr);
            return *this;
        }
#endif

        inline ~socket_t ()
        {
            close();
        }

        inline operator void* ()
        {
            return ptr;
        }

        inline void close()
        {
            if(ptr == NULL)
                return ;

            int rc = xs_close (ptr);
            XS_ASSERT (rc == 0);
            ptr = 0 ;
        }

        inline void setsockopt (int option_, const void *optval_,
            size_t optvallen_)
        {
            int rc = xs_setsockopt (ptr, option_, optval_, optvallen_);
            if (rc != 0)
                throw error_t ();
        }

        inline void getsockopt (int option_, void *optval_,
            size_t *optvallen_)
        {
            int rc = xs_getsockopt (ptr, option_, optval_, optvallen_);
            if (rc != 0)
                throw error_t ();
        }

        inline int bind (const char *addr_)
        {
            int rc = xs_bind (ptr, addr_);
            if (rc == -1)
                throw error_t ();
            return rc;
        }

        inline int connect (const char *addr_)
        {
            int rc = xs_connect (ptr, addr_);
            if (rc == -1)
                throw error_t ();
            return rc;
        }

        inline void shutdown (int how_)
        {
            int rc = xs_shutdown (ptr, how_);
            if (rc != 0)
                throw error_t ();
        }

        inline size_t send (const void *buf_, size_t len_, int flags_ = 0)
        {
            int nbytes = xs_send (ptr, buf_, len_, flags_);
            if (nbytes >= 0)
                return (size_t) nbytes;
            if (xs_errno () == EAGAIN)
                return 0;
            throw error_t ();
        }

        inline bool send (message_t &msg_, int flags_ = 0)
        {
            int nbytes = xs_sendmsg (ptr, &(msg_.msg), flags_);
            if (nbytes >= 0)
                return true;
            if (xs_errno () == EAGAIN)
                return false;
            throw error_t ();
        }

        inline size_t recv (void *buf_, size_t len_, int flags_ = 0)
        {
            int nbytes = xs_recv (ptr, buf_, len_, flags_);
            if (nbytes >= 0)
                return (size_t) nbytes;
            if (xs_errno () == EAGAIN)
                return 0;
            throw error_t ();
        }

        inline bool recv (message_t *msg_, int flags_ = 0)
        {
            int nbytes = xs_recvmsg (ptr, &(msg_->msg), flags_);
            if (nbytes >= 0)
                return true;
            if (xs_errno () == EAGAIN)
                return false;
            throw error_t ();
        }

    private:

        void *ptr;

        socket_t (const socket_t&);
        void operator = (const socket_t&);
    };

}

#endif

