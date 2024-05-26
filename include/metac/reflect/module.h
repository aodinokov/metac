#ifndef INCLUDE_METAC_REFLECT_MODULE_H_
#define INCLUDE_METAC_REFLECT_MODULE_H_

#ifndef METAC_MODULE_NAME
#define METAC_MODULE_NAME dflt
#endif

/** @brief set module name if it's not default. see reqresp.h for more information */
#define METAC_MODULE(name) char * METAC_REQUEST(module, name) = #name

#endif