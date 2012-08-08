/*
 * Copyright (C) 2012 FRAFOS GmbH
 *
 * Development sponsored by Sipwise GmbH.
 *
 * This file is part of SEMS, a free SIP media server.
 *
 * SEMS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. This program is released under
 * the GPL with the additional exemption that compiling, linking,
 * and/or using OpenSSL is allowed.
 *
 * For a license to use the SEMS software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * SEMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "ModXml.h"
#include "log.h"
#include "AmUtils.h"

#include <string.h>

SC_EXPORT(MOD_CLS_NAME);

int MOD_CLS_NAME::preload() {
  DBG("initializing libxml2...\n");
  xmlInitParser();

  return 0;
}

MOD_ACTIONEXPORT_BEGIN(MOD_CLS_NAME) {
  DEF_CMD("xml.parse", MODXMLParseAction);
  DEF_CMD("xml.parseSIPMsgBody", MODXMLParseSIPMsgBodyAction);

  DEF_CMD("xml.evalXPath", MODXMLEvalXPathAction);
  DEF_CMD("xml.XPathResultCount", MODXMLXPathResultNodeCount);

} MOD_ACTIONEXPORT_END;

MOD_CONDITIONEXPORT_NONE(MOD_CLS_NAME);

ModXmlDoc::~ModXmlDoc() {
  if (NULL != doc) {
    DBG("freeing XML document [%p]\n", doc);
    xmlFreeDoc(doc);
  }
}

ModXmlXPathObj::~ModXmlXPathObj() {
  if (NULL != xpathObj) {
    DBG("freeing XML xpath obj [%p]\n", xpathObj);
    xmlXPathFreeObject(xpathObj);
  }
  if (NULL != xpathCtx) {
    DBG("freeing XML xpath ctx [%p]\n", xpathCtx);
    xmlXPathFreeContext(xpathCtx);
  }
}

CONST_ACTION_2P(MODXMLParseSIPMsgBodyAction, ',', false);
EXEC_ACTION_START(MODXMLParseSIPMsgBodyAction) {
  string msgbody_var = resolveVars(par1, sess, sc_sess, event_params);
  string dstname = resolveVars(par2, sess, sc_sess, event_params);
  AVarMapT::iterator it = sc_sess->avar.find(msgbody_var);
  if (it==sc_sess->avar.end()) {
    DBG("no message body in avar '%s'\n", msgbody_var.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("no message body in avar " + msgbody_var);
    EXEC_ACTION_STOP;
  }
  if (!isArgCStr(it->second)) {
    DBG("no SIP MSG body in avar '%s'\n", msgbody_var.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("no SIP MSG body in avar " + msgbody_var);
    EXEC_ACTION_STOP;
  }
  const char* b =  it->second.asCStr();
  if (b==NULL) {
    DBG("empty SIP MSG body in avar '%s'\n", msgbody_var.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("empty SIP MSG body in avar " + msgbody_var);
    EXEC_ACTION_STOP;
  }

  xmlDocPtr doc =
    xmlReadMemory((const char*)b, strlen(b), "noname.xml", NULL, 0);
  if (doc == NULL) {
    DBG("failed parsing XML document from '%s'\n", msgbody_var.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("failed parsing XML document from " + msgbody_var);
    EXEC_ACTION_STOP;
  }

  ModXmlDoc* xml_doc = new ModXmlDoc(doc);
  sc_sess->avar[dstname] = xml_doc;
  DBG("parsed XML body document to '%s'\n", dstname.c_str());

//  string basedir = resolveVars(par2, sess, sc_sess, event_params);
} EXEC_ACTION_END;

CONST_ACTION_2P(MODXMLParseAction, ',', false);
EXEC_ACTION_START(MODXMLParseAction) {
  string xml_doc = resolveVars(par1, sess, sc_sess, event_params);
  string dstname = resolveVars(par2, sess, sc_sess, event_params);

  xmlDocPtr doc =
    xmlReadMemory(xml_doc.c_str(), xml_doc.length(), "noname.xml", NULL, 0);
  if (doc == NULL) {
    DBG("failed parsing XML document from '%s'\n", xml_doc.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("failed parsing XML document from " + xml_doc);
    EXEC_ACTION_STOP;
  }

  ModXmlDoc* xml_doc_var = new ModXmlDoc(doc);
  sc_sess->avar[dstname] = xml_doc_var;
  DBG("parsed XML body document to '%s'\n", dstname.c_str());
} EXEC_ACTION_END;

template<class T>
T* getXMLElemFromVariable(DSMSession* sc_sess, const string& var_name) {
  AVarMapT::iterator it = sc_sess->avar.find(var_name);
  if (it == sc_sess->avar.end()) {
    DBG("object '%s' not found\n", var_name.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("object '"+var_name+"' not found\n");
    return NULL;
  }

  T* doc = dynamic_cast<T*>(it->second.asObject());
  if (NULL == doc) {
    DBG("object '%s' is not the right type\n", var_name.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("object '"+var_name+"' is not the right type\n");
    return NULL;
  }
  return doc;
}

CONST_ACTION_2P(MODXMLEvalXPathAction, ',', false);
EXEC_ACTION_START(MODXMLEvalXPathAction) {
  string xpath_expr  = resolveVars(par1, sess, sc_sess, event_params);
  string xml_doc_var = resolveVars(par2, sess, sc_sess, event_params);

  ModXmlDoc* xml_doc = getXMLElemFromVariable<ModXmlDoc>(sc_sess, xml_doc_var);
  if (NULL == xml_doc)
    EXEC_ACTION_STOP;

  xmlDocPtr doc = xml_doc->doc;
  
  xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
  if(xpathCtx == NULL) {
    DBG("unable to create new XPath context\n");
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("unable to create new XPath context");
    EXEC_ACTION_STOP;
  }

  string xml_doc_ns = sc_sess->var[xml_doc_var+".ns"];
  vector<string> ns_entries = explode(xml_doc_ns, " ");
  for (vector<string>::iterator it=ns_entries.begin(); it != ns_entries.end(); it++) {
    vector<string> ns = explode(*it, "=");
    if (ns.size() != 2) {
      DBG("script writer error: namespace entry must be prefix=href (got '%s')\n",
	  it->c_str());
      sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
      sc_sess->SET_STRERROR("script writer error: namespace entry must be prefix=href\n");
      xmlXPathFreeContext(xpathCtx);
      EXEC_ACTION_STOP;
    }

    if(xmlXPathRegisterNs(xpathCtx, (const xmlChar*)ns[0].c_str(),
			  (const xmlChar*)ns[1].c_str()) != 0) {
      DBG("unable to register namespace %s=%s\n", ns[0].c_str(), ns[1].c_str());
      sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
      sc_sess->SET_STRERROR("unable to register namespace\n");
      xmlXPathFreeContext(xpathCtx);
      EXEC_ACTION_STOP;
    }
    DBG("registered namespace %s=%s\n", ns[0].c_str(), ns[1].c_str());
  }

  xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((const xmlChar*)xpath_expr.c_str(),
						      xpathCtx);
  if(xpathObj == NULL) {
    DBG("unable to evaluate xpath expression \"%s\"\n", xpath_expr.c_str());
    sc_sess->SET_ERRNO(DSM_ERRNO_UNKNOWN_ARG);
    sc_sess->SET_STRERROR("unable to evaluate xpath expression");
    xmlXPathFreeContext(xpathCtx);
    EXEC_ACTION_STOP;
  }

  ModXmlXPathObj* xpath_obj = new ModXmlXPathObj(xpathObj, xpathCtx);
  sc_sess->avar[xml_doc_var+".xpath"] = xpath_obj;
  DBG("evaluated XPath expression on '%s' to '%s'\n",
      xml_doc_var.c_str(), (xml_doc_var+".xpath").c_str());

} EXEC_ACTION_END;


CONST_ACTION_2P(MODXMLXPathResultNodeCount, '=', false);
EXEC_ACTION_START(MODXMLXPathResultNodeCount) {
  string cnt_var  = par1;
  string xpath_res_var = resolveVars(par2, sess, sc_sess, event_params);

  if (cnt_var.size() && cnt_var[0]=='$') {
    cnt_var.erase(0,1);
  }

  ModXmlXPathObj* xpath_obj =
    getXMLElemFromVariable<ModXmlXPathObj>(sc_sess, xpath_res_var);
  if (NULL == xpath_obj){
    DBG("no xpath result found in '%s'\n", xpath_res_var.c_str());
    sc_sess->var[cnt_var] = "0";
    EXEC_ACTION_STOP;
  }

  unsigned int res = (xpath_obj->xpathObj->nodesetval) ? 
    xpath_obj->xpathObj->nodesetval->nodeNr : 0;

  sc_sess->var[cnt_var] = int2str(res);
  DBG("set count $%s=%u\n", cnt_var.c_str(), res);
  
} EXEC_ACTION_END;
