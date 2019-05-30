#include "entry.h"

namespace posix_quic {

FdFactory & EntryBase::GetFdFactory()
{
    static FdFactory *obj = new FdFactory;
    return *obj;
}

FdManager<EntryPtr> & EntryBase::GetFdManager()
{
    static FdManager<EntryPtr> *obj = new FdManager<EntryPtr>;
    return *obj;
}

} // namespace posix_quic
