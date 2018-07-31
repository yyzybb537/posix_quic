#include "entry.h"

namespace posix_quic {

FdFactory & EntryBase::GetFdFactory()
{
    static FdFactory obj;
    return obj;
}
FdManager<EntryPtr> & EntryBase::GetFdManager()
{
    static FdManager<EntryPtr> obj;
    return obj;
}

} // namespace posix_quic
