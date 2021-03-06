 /*
 * \ file pygetdns_util.c
 * @brief utility functions to support pygetdns bindings
 */


/*
 * Copyright (c) 2014, Verisign, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the <organization> nor the
 * names of its contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Verisign, Include. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <Python.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <getdns/getdns.h>
#include <ldns/ldns.h>
#include "pygetdns.h"

struct getdns_dict *
extensions_to_getdnsdict(PyDictObject *pydict)
{
    struct getdns_dict *newdict = 0;
    Py_ssize_t pos = 0, optiondictpos = 0, optionlistpos = 0;
    PyObject *key, *value;
    char *tmpoptionlistkey;
    struct getdns_list *optionslist = 0;         /* for options list */
    int optionlistsize;                   /* how many options in options list */
    int i;                                /* loop counter */
    PyObject *optionitem;
    PyObject *optiondictkey, *optiondictvalue; /* for processing option list dicts */
    struct getdns_bindata *option_data;
    struct getdns_dict *tmpoptions_list_dict; /* a dict to hold add_opt_parameters[options] stuff */

    if (!PyDict_Check(pydict))  {
        PyErr_SetString(getdns_error, "Expected dict, didn't get one");
        return NULL;
    }
    newdict = getdns_dict_create(); /* this is what we'll return */

    while (PyDict_Next((PyObject *)pydict, &pos, &key, &value))  { /* these options take TRUE or FALSE args */
        char *tmp_key;
        int  tmp_int;

        tmp_key = PyString_AsString(PyObject_Str(key));
        if ( (!strncmp(tmp_key, "dnssec_return_status", strlen("dnssec_return_status")))  ||
             (!strncmp(tmp_key, "dnssec_return_only_secure", strlen("dnssec_return_only_secure")))  ||
             (!strncmp(tmp_key, "dnssec_return_validation_chain", strlen("dnssec_return_validation_chain")))  ||
             (!strncmp(tmp_key, "return_both_v4_and_v6", strlen("return_both_v4_and_v6")))  ||
             (!strncmp(tmp_key, "dnssec_return_supporting_responses", strlen("dnssec_return_supporting_responses")))  ||
             (!strncmp(tmp_key, "return_api_information", strlen("return_api_information")))  ||
             (!strncmp(tmp_key, "return_call_debugging", strlen("return_call_debugging")))  ||
             (!strncmp(tmp_key, "add_warning_for_bad_dns", strlen("add_warning_for_bad_dns"))) )  {
            if (!PyInt_Check(value))  {
                PyErr_SetString(getdns_error, GETDNS_RETURN_EXTENSION_MISFORMAT_TEXT);
                return NULL;
            }
            if ( !((PyInt_AsLong(value) == GETDNS_EXTENSION_TRUE) || (PyInt_AsLong(value) == GETDNS_EXTENSION_FALSE)) )  {
                PyErr_SetString(getdns_error, GETDNS_RETURN_EXTENSION_MISFORMAT_TEXT);
                return NULL;
            }
            tmp_int = (int)PyInt_AsLong(value);

            (void)getdns_dict_set_int(newdict, tmp_key, tmp_int);
        } else if (!strncmp(tmp_key, "specify_class", strlen("specify_class")))  { /* takes integer */
            if (!PyInt_Check(value))  {
                PyErr_SetString(getdns_error, GETDNS_RETURN_EXTENSION_MISFORMAT_TEXT);
                return NULL;
            }
            tmp_int = (int)PyInt_AsLong(value);
            (void)getdns_dict_set_int(newdict, tmp_key, tmp_int);

/*
 *  dns OPT resource record setup
 *
 *    extensions['add_opt_parameters'][option_name]
 */


        } else if (!strncmp(tmp_key, "add_opt_parameters", strlen("add_opt_parameters")))  { /* this is a dict */
            PyObject *in_optdict; /* points at dictionary passed in */
            struct getdns_dict *out_optdict = 0;
            Py_ssize_t opt_pos = 0;
            PyObject *opt_key, *opt_value;
            char *tmp_opt_key;
            int optint;

            in_optdict = value;
            if (!PyDict_Check(in_optdict))  {
                PyErr_SetString(getdns_error, "Expected dict, didn't get one");
                return NULL;
            }
            out_optdict = getdns_dict_create();
            while (PyDict_Next((PyObject *)in_optdict, &opt_pos, &opt_key, &opt_value))  {
                tmp_opt_key = PyString_AsString(opt_key);
                if ( (!strncmp(tmp_opt_key, "maximum_udp_payload_size", strlen("maximum_udp_payload_size")))  ||
                     (!strncmp(tmp_opt_key, "extended_rcode", strlen("extended_rcode"))) ||
                     (!strncmp(tmp_opt_key, "version", strlen("version"))) ||
                     (!strncmp(tmp_opt_key, "do_bit", strlen("do_bit"))) )  {
                    if (!PyInt_Check(opt_value))  {
                        PyErr_SetString(getdns_error, GETDNS_RETURN_EXTENSION_MISFORMAT_TEXT);
                        return NULL;
                    }
                    optint = (int)PyInt_AsLong(opt_value);
                    (void)getdns_dict_set_int(out_optdict, tmp_opt_key, optint);
                }  else if (!strncmp(tmp_opt_key, "options", strlen("options")))  { /* options */
/*
 * options with arbitrary opt code
 *
 *    add_opt_parameters is a dict containing
 *      options is a list containing
 *        dicts for each option containing
 *          option_code (int)
 *          option_data (bindata)
 *    
 */

                    if (!PyList_Check(opt_value))  {
                        PyErr_SetString(getdns_error, GETDNS_RETURN_EXTENSION_MISFORMAT_TEXT);
                        return NULL;
                    }
                    optionslist = getdns_list_create();

                    optionlistsize = PyList_Size(opt_value);

                    for ( i = 0 ; i < optionlistsize ; i++)  {
                        tmpoptions_list_dict = getdns_dict_create();
                        optionitem = PyList_GetItem(opt_value, i);
                        if (!PyDict_Check(optionitem))  {
                            PyErr_SetString(getdns_error, GETDNS_RETURN_EXTENSION_MISFORMAT_TEXT);
                            return NULL;
                        }
                        /* optionitem should be a dict with keys option_code and option_data */
                        while (PyDict_Next(optionitem, &optiondictpos, &optiondictkey, &optiondictvalue))  {
                            tmpoptionlistkey = PyString_AsString(PyObject_Str(optiondictkey));
                            if  (!strncmp(tmpoptionlistkey, "option_code", strlen("option_code")))  {
                                if (!PyInt_Check(optiondictvalue))  {
                                    PyErr_SetString(getdns_error, GETDNS_RETURN_EXTENSION_MISFORMAT_TEXT);
                                    return NULL;
                                }
                                getdns_dict_set_int(tmpoptions_list_dict, "option_code", (uint32_t)PyInt_AsLong(optiondictvalue));
                            }  else if (!strncmp(tmpoptionlistkey, "option_data", strlen("option_data")))  {
                                option_data = (struct getdns_bindata *)malloc(sizeof(struct getdns_bindata));
                                option_data->size = PyObject_Length(optiondictvalue);
                                option_data->data = (uint8_t *)PyString_AsString(PyObject_Bytes(optiondictvalue)); /* This is almost certainly wrong */
                                getdns_dict_set_bindata(tmpoptions_list_dict, "option_data", option_data);
                            } else  {
                                PyErr_SetString(getdns_error, GETDNS_RETURN_EXTENSION_MISFORMAT_TEXT);
                                return NULL;
                            }
                            getdns_list_set_dict(optionslist, optionlistpos, tmpoptions_list_dict);
                        }
                    } /* for i ... optionlistsize */
                    getdns_dict_set_list(out_optdict, "options", optionslist);
                }     /* for options */
                getdns_dict_set_dict(newdict, "add_opt_parameters", out_optdict);
            } /* while PyDict_Next(tmp_optdict ... ) */
        } else {
            PyErr_SetString(getdns_error, GETDNS_RETURN_NO_SUCH_EXTENSION_TEXT);
            return NULL;
        }
    }
    return newdict;
}


/*
 * turn a Python address dictionary into a getdns data structure (inc. validation)
 */


getdns_dict *
getdnsify_addressdict(PyObject *pydict)
{
    getdns_dict *addr_dict;
    getdns_bindata addr_data;
    getdns_bindata addr_type;
    PyObject *str;
    unsigned char buf[sizeof(struct in6_addr)];
    int domain;


    if (!PyDict_Check(pydict))  {
        PyErr_SetString(getdns_error, GETDNS_RETURN_INVALID_PARAMETER_TEXT);
        return NULL;
    }
    if (PyDict_Size(pydict) != 2)  {
        PyErr_SetString(getdns_error, GETDNS_RETURN_INVALID_PARAMETER_TEXT);
        return NULL;
    }
    addr_dict = getdns_dict_create();
    if ((str = PyDict_GetItemString(pydict, "address_type")) == NULL)  {
        PyErr_SetString(getdns_error, GETDNS_RETURN_INVALID_PARAMETER_TEXT);
        return NULL;
    }
    if (!PyString_Check(str))  {
        PyErr_SetString(getdns_error, GETDNS_RETURN_INVALID_PARAMETER_TEXT);
        return NULL;
    }
    addr_type.data = (uint8_t *)strdup(PyString_AsString(str));
    addr_type.size = strlen((char *)addr_type.data);
    if (strlen((char *)addr_type.data) != 4)  {
        PyErr_SetString(getdns_error, GETDNS_RETURN_WRONG_TYPE_REQUESTED_TEXT);
        return NULL;
    }
    if (!strncasecmp((char *)addr_type.data, "IPv4", 4))
        domain = AF_INET;
    else if (!strncasecmp((char *)addr_type.data, "IPv6", 4))
        domain = AF_INET6;
    else  {
        PyErr_SetString(getdns_error,  GETDNS_RETURN_INVALID_PARAMETER_TEXT);
        return NULL;
    }
    getdns_dict_set_bindata(addr_dict, "address_type", &addr_type);

    if ((str = PyDict_GetItemString(pydict, "address_data")) == NULL)  {
        PyErr_SetString(getdns_error, GETDNS_RETURN_INVALID_PARAMETER_TEXT);
        return NULL;
    }
    if (!PyString_Check(str))  {
        PyErr_SetString(getdns_error, GETDNS_RETURN_INVALID_PARAMETER_TEXT);
        return NULL;
    }
    if (inet_pton(domain, PyString_AsString(str), buf) <= 0)  {
        PyErr_SetString(getdns_error, GETDNS_RETURN_INVALID_PARAMETER_TEXT);
        return NULL;
    }
    addr_data.data = (uint8_t *)buf;
    addr_data.size = (domain == AF_INET ? 4 : 16);
    getdns_dict_set_bindata(addr_dict, "address_data", &addr_data);
    return addr_dict;
}


PyObject *
decode_getdns_response(struct getdns_dict *response)
{
    uint32_t error;
    char error_str[512];
    struct getdns_list *addr_list;
    size_t n_addrs;
    getdns_return_t ret;
    size_t i;
    struct getdns_dict *addr_dict;
    struct getdns_bindata *addr;
    char *addr_str;
    PyObject *results;

    (void)getdns_dict_get_int(response, "status", &error);
    if (error != GETDNS_RESPSTATUS_GOOD)  {
        sprintf(error_str, "No answers, return value %d", error);
        PyErr_SetString(getdns_error, error_str);
        return NULL;
    }
    if ((ret = getdns_dict_get_list(response, "just_address_answers",
                                    &addr_list)) != GETDNS_RETURN_GOOD)  {
        sprintf(error_str, "Extracting answers failed: %d", ret);
        PyErr_SetString(getdns_error, error_str);
        return NULL;
    }
    if ((results = PyList_New(0)) == NULL)  {
        PyErr_SetString(getdns_error, "Unable to allocate response list");
        return NULL;
    }
    (void)getdns_list_get_length(addr_list, &n_addrs);
    for ( i = 0 ; i < n_addrs ; i++ )  {
        (void)getdns_list_get_dict(addr_list, i, &addr_dict);
        (void)getdns_dict_get_bindata(addr_dict, "address_data", &addr);
        addr_str = getdns_display_ip_address(addr);
        PyList_Append(results, PyString_FromString(addr_str));
    }
    return results;
}


/*
 * Error checking helper
 */
void error_exit(char* msg, getdns_return_t ret)
{
    char error_str[512];
    if (ret != GETDNS_RETURN_GOOD)  {
        sprintf(error_str, "%s: %d", msg, ret);
        printf("ERROR: %s: %d", msg, ret);
        PyErr_SetString(getdns_error, error_str);
    }
}



/**
 * reverse an IP address for PTR lookup
 * @param address_data IP address to reverse
 * @return NULL on allocation failure
 * @return reversed string on success, caller must free storage via call to free()
 */
char *
reverse_address(struct getdns_bindata *address_data)
{
    ldns_rdf *addr_rdf;
    ldns_rdf *rev_rdf;
    char *rev_str;

    if (address_data->size == 4)
        addr_rdf = ldns_rdf_new(LDNS_RDF_TYPE_A, 4, address_data->data);
    else if (address_data->size == 16)
        addr_rdf = ldns_rdf_new(LDNS_RDF_TYPE_AAAA, 16, address_data->data);
    else
        return NULL;
    if (!addr_rdf)
        return NULL;

    rev_rdf = ldns_rdf_address_reverse(addr_rdf);
    ldns_rdf_free(addr_rdf);
    if (!rev_rdf)
        return NULL;

    rev_str = ldns_rdf2str(rev_rdf);
    ldns_rdf_deep_free(rev_rdf);
    return rev_str;
}


// Code to display the entire response.
// answer_type
// canonical_name
// just_address_answers
// replies_full
// replies_tree

// Taken from getdns source to do label checking
static int
priv_getdns_bindata_is_dname(struct getdns_bindata *bindata)
{
    size_t i = 0, n_labels = 0;
    while (i < bindata->size) {
        i += ((size_t)bindata->data[i]) + 1;
        n_labels++;
    }
    return i == bindata->size && n_labels > 1 &&
        bindata->data[bindata->size - 1] == 0;
}

// Convert bindata into a good representational string or
// into a buffer. Handles dname, printable, ".",
// and an ip address if it is under a known key

PyObject *
convertBinData(getdns_bindata* data,
                    const char* key) {


    size_t i; 


    // the root
    if (data->size == 1 && data->data[0] == 0) {
        PyObject *a_string;

        if ((a_string = PyString_FromString(".")) == NULL)  {
            PyErr_SetString(getdns_error, GETDNS_RETURN_GENERIC_ERROR_TEXT);
            return NULL;
        }
        return(a_string);
    }

    int printable = 1;
    for (i = 0; i < data->size; ++i) {
        if (!isprint(data->data[i])) {
            if (data->data[i] == 0 &&
                i == data->size - 1) {
                break;
            }
            printable = 0;
            break;
        }
    }
    // basic string?
    if (printable == 1) {
        PyObject *a_string;

        if ((a_string = PyString_FromString((char *)data->data)) == NULL)  {
            PyErr_SetString(getdns_error, GETDNS_RETURN_GENERIC_ERROR_TEXT);
            return NULL;
        }
        return(a_string);
    }


    // dname
    if (priv_getdns_bindata_is_dname(data)) {
        char* dname = NULL;
        PyObject *dname_string;

        if (getdns_convert_dns_name_to_fqdn(data, &dname)
            == GETDNS_RETURN_GOOD) {
            if ((dname_string = PyString_FromString(dname)) != NULL)  {
                return(dname_string);
            }  else  {
                PyErr_SetString(getdns_error, GETDNS_RETURN_GENERIC_ERROR_TEXT);
                return NULL;
            }
        } else {
            PyErr_SetString(getdns_error, GETDNS_RETURN_GENERIC_ERROR_TEXT);
            return NULL;
        }
    } else if (key != NULL &&
        (strcmp(key, "ipv4_address") == 0 ||
         strcmp(key, "ipv6_address") == 0)) {
        char* ipStr = getdns_display_ip_address(data);
        if (ipStr) {
            PyObject *addr_string;
            if ((addr_string = PyString_FromString(ipStr)) == NULL)  {
                PyErr_SetString(getdns_error, GETDNS_RETURN_GENERIC_ERROR_TEXT);
                return NULL;
            }
            return(addr_string);
        }
    }  else  {                  /* none of the above, treat it like a blob */
#if 0
        Py_buffer pybuf;
#endif
        uint8_t *blob = (uint8_t *)malloc(data->size);

        memcpy(blob, data->data, data->size);
        return (PyBuffer_FromMemory(blob, (Py_ssize_t)data->size));
#if 0
        if (PyBuffer_FillInfo(&pybuf, 0, blob, data->size, false, PyBUF_CONTIG) < 0)  {
            PyErr_SetString(getdns_error, "PyBuffer_FillInfo failed\n");
            return NULL;
        }
        return PyMemoryView_FromBuffer(&pybuf);
#endif
    }
    return NULL;                /* should never get here .. */
}

char* 
getdns_dict_to_ip_string(getdns_dict* dict);
PyObject* 
convertToList(struct getdns_list* list);

PyObject*
convertToDict(struct getdns_dict* dict) {

    if (!dict) {
        return 0;
    }

    PyObject* resultsdict1;
    if ((resultsdict1 = PyDict_New()) == NULL)  {
        error_exit("Unable to allocate response dict", 0);
        return NULL;
    }

    // try it as an IP
    char* ipStr = getdns_dict_to_ip_string(dict);
    if (ipStr) {
        PyObject* res1 = Py_BuildValue("s", ipStr);
        PyDict_SetItem(resultsdict1, PyString_FromString("IPSTRING"), res1);
        return resultsdict1;
    }

    getdns_list* names;
    getdns_dict_get_names(dict, &names);
    size_t len = 0, i = 0;
    getdns_list_get_length(names, &len);
    for (i = 0; i < len; ++i) {
        getdns_bindata* nameBin;
        getdns_list_get_bindata(names, i, &nameBin);
        getdns_data_type type;
        getdns_dict_get_data_type(dict, (char*)nameBin->data, &type);
        switch (type) {
            case t_bindata:
            {
                getdns_bindata* data = NULL;
                getdns_dict_get_bindata(dict, (char*)nameBin->data, &data);
                PyObject* res = convertBinData(data, (char*)nameBin->data);
                PyDict_SetItem(resultsdict1, PyString_FromString((char*) nameBin->data), res);
                break;
            }
            case t_int:
            {
                uint32_t res = 0;
                getdns_dict_get_int(dict, (char*)nameBin->data, &res);
                PyObject* rl1 = Py_BuildValue("i", res);
                PyObject *res1 = Py_BuildValue("O", rl1);
                PyDict_SetItem(resultsdict1, PyString_FromString((char*) nameBin->data), res1);
                break;
            }
            case t_dict:
            {
                getdns_dict* subdict = NULL;
                getdns_dict_get_dict(dict, (char*)nameBin->data, &subdict);
                PyObject *rl1 = convertToDict(subdict);
                PyObject *res1 = Py_BuildValue("O", rl1);
                PyDict_SetItem(resultsdict1, PyString_FromString((char*) nameBin->data), res1);
                break;
            }
            case t_list:
            {
                getdns_list* list = NULL;
                getdns_dict_get_list(dict, (char*)nameBin->data, &list);
                PyObject *rl1 = convertToList(list);
                PyObject *res1 = Py_BuildValue("O", rl1);
                PyObject *key = PyString_FromString((char *)nameBin->data);
                PyDict_SetItem(resultsdict1, key, res1);
                break;
            }
            default:
                break;
        }
    }
    getdns_list_destroy(names);
    return resultsdict1;
}



PyObject* 
convertToList(struct getdns_list* list) {


    if (!list) {
        return 0;
    }

    PyObject* resultslist1;
    if ((resultslist1 = PyList_New(0)) == NULL)  {
        error_exit("Unable to allocate response list", 0);
        return NULL;
    }

    size_t len, i;
    getdns_list_get_length(list, &len);
    for (i = 0; i < len; ++i) {
        getdns_data_type type;
        getdns_list_get_data_type(list, i, &type);
        switch (type) {
            case t_bindata:
            {
                getdns_bindata* data = NULL;
                getdns_list_get_bindata(list, i, &data);
                PyObject* res = convertBinData(data, NULL);
                if (res) {
                    PyList_Append(resultslist1, res);
                } else {

                    PyObject* res1 = Py_BuildValue("s", "empty");
                    PyList_Append(resultslist1, res1);
                }
                break;
            }
            case t_int:
            {
                uint32_t res = 0;
                getdns_list_get_int(list, i, &res);
                PyObject* res1 = Py_BuildValue("i", res); 
                PyList_Append(resultslist1, res1);
                break;
            }
            case t_dict:
            {
                getdns_dict* dict = NULL;
                getdns_list_get_dict(list, i, &dict);
                PyObject *rl1 = convertToDict(dict);
                PyObject *res1 = Py_BuildValue("O", rl1);
                PyList_Append(resultslist1, res1);
                break;
            }
            case t_list:
            {
                getdns_list* sublist = NULL;
                getdns_list_get_list(list, i, &sublist);
                PyObject* rl1 = convertToList(sublist);
                PyObject *res1 = Py_BuildValue("O", rl1);
                PyList_Append(resultslist1, res1);
                break;
            }
            default:
                break;
        }
    }

    return resultslist1;
}

// potential helper to get the ip string of a dict
char* getdns_dict_to_ip_string(getdns_dict* dict) {
    if (!dict) {
        return NULL;
    }
    getdns_bindata* data;
    getdns_return_t r;
    r = getdns_dict_get_bindata(dict, "address_type", &data);
    if (r != GETDNS_RETURN_GOOD) {
        return NULL;
    }
    if (data->size == 5 &&
        (strcmp("IPv4", (char*) data->data) == 0 ||
         strcmp("IPv6", (char*) data->data) == 0)) {
        r = getdns_dict_get_bindata(dict, "address_data", &data);
        if (r != GETDNS_RETURN_GOOD) {
            return NULL;
        }
        return getdns_display_ip_address(data);
    }
    return NULL;
}

PyObject*
getFullResponse(struct getdns_dict *dict)
{

    PyObject* resultslist;
    if ((resultslist = PyDict_New()) == NULL)  {
        error_exit("Unable to allocate response list", 0);
        return NULL;
    }

    getdns_list* names;
    getdns_dict_get_names(dict, &names);
    size_t len = 0, i = 0;
    getdns_list_get_length(names, &len);
    for (i = 0; i < len; ++i) {
        getdns_bindata* nameBin;
        getdns_list_get_bindata(names, i, &nameBin);
        getdns_data_type type;
        getdns_dict_get_data_type(dict, (char*)nameBin->data, &type);
        switch (type) {
            case t_bindata:
            {
                getdns_bindata* data = NULL;
                getdns_dict_get_bindata(dict, (char*)nameBin->data, &data);
                PyObject *res = convertBinData(data, (char*)nameBin->data);
                if (res) {
                   PyDict_SetItem(resultslist, PyString_FromString((char*)nameBin->data), res);
                } else {
                   PyObject* res1 = Py_BuildValue("s", "empty");
                   PyDict_SetItem(resultslist, PyString_FromString((char*)nameBin->data), res1);
                }
                break;
            }
            case t_int:
            {
                uint32_t res = 0;
                getdns_dict_get_int(dict, (char*)nameBin->data, &res);
                PyObject* res1 = Py_BuildValue("i", res);
                PyDict_SetItem(resultslist, PyString_FromString((char*)nameBin->data), res1);
                break;
            }
            case t_dict:
            {
                getdns_dict* subdict = NULL;
                getdns_dict_get_dict(dict, (char*)nameBin->data, &subdict);
                PyObject* rl1 = convertToDict(subdict);
                PyObject *res1 = Py_BuildValue("O", rl1);
                PyDict_SetItem(resultslist, PyString_FromString((char*)nameBin->data), res1);
                break;
            }
            case t_list:
            {
                getdns_list* list = NULL;
                getdns_dict_get_list(dict, (char*)nameBin->data, &list);
                PyObject* rl1 = convertToList(list);
                PyObject *res1 = Py_BuildValue("O", rl1);
                PyDict_SetItem(resultslist, PyString_FromString((char*)nameBin->data), res1);
                break;
            }
            default:
                break;
        }
    }

    getdns_list_destroy(names);

    return resultslist;
}

