/* datalog89_status.c - status inspection (CONVENTIONS.md section 14).
 *
 * Both functions are total: an unknown integer value yields a defined
 * fallback. Neither allocates; both return immutable static storage.
 */

#include <datalog89.h>

const char *datalog89_status_name(datalog89_status status)
{
    if (status == DATALOG89_OK)
    {
        return "OK";
    }
    if (status == DATALOG89_EINVAL)
    {
        return "EINVAL";
    }
    if (status == DATALOG89_ENOMEM)
    {
        return "ENOMEM";
    }
    if (status == DATALOG89_EPROGRAM)
    {
        return "EPROGRAM";
    }
    if (status == DATALOG89_ESTORE)
    {
        return "ESTORE";
    }
    if (status == DATALOG89_EBUSY)
    {
        return "EBUSY";
    }
    return "UNKNOWN";
}

const char *datalog89_status_message(datalog89_status status)
{
    if (status == DATALOG89_OK)
    {
        return "success";
    }
    if (status == DATALOG89_EINVAL)
    {
        return "invalid argument";
    }
    if (status == DATALOG89_ENOMEM)
    {
        return "allocation failed";
    }
    if (status == DATALOG89_EPROGRAM)
    {
        return "invalid rule or arity conflict";
    }
    if (status == DATALOG89_ESTORE)
    {
        return "store callback failed";
    }
    if (status == DATALOG89_EBUSY)
    {
        return "mutation during a run";
    }
    return "unknown status";
}
