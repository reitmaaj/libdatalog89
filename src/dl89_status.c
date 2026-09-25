/* dl89_status.c - status inspection (CONVENTIONS.md section 14).
 *
 * Both functions are total: an unknown integer value yields a defined
 * fallback. Neither allocates; both return immutable static storage.
 */

#include <dl89.h>

const char *dl89_status_name(dl89_status status)
{
    if (status == DL89_OK)
    {
        return "OK";
    }
    if (status == DL89_EINVAL)
    {
        return "EINVAL";
    }
    if (status == DL89_ENOMEM)
    {
        return "ENOMEM";
    }
    if (status == DL89_EPROGRAM)
    {
        return "EPROGRAM";
    }
    if (status == DL89_ESTORE)
    {
        return "ESTORE";
    }
    if (status == DL89_EBUSY)
    {
        return "EBUSY";
    }
    return "UNKNOWN";
}

const char *dl89_status_message(dl89_status status)
{
    if (status == DL89_OK)
    {
        return "success";
    }
    if (status == DL89_EINVAL)
    {
        return "invalid argument";
    }
    if (status == DL89_ENOMEM)
    {
        return "allocation failed";
    }
    if (status == DL89_EPROGRAM)
    {
        return "invalid rule or arity conflict";
    }
    if (status == DL89_ESTORE)
    {
        return "store callback failed";
    }
    if (status == DL89_EBUSY)
    {
        return "mutation during a run";
    }
    return "unknown status";
}
