<!--
    Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
   
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
-->

<xsl:stylesheet version="3.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xs="http://www.w3.org/2001/XMLSchema" exclude-result-prefixes="xs" expand-text="yes">

  <xsl:variable name="doc-ns" select="'boost::mysql'"/>
  <xsl:variable name="doc-ref" select="'mysql.ref'"/>
  <xsl:variable name="debug" select="0"/>
  <xsl:variable name="private" select="0"/>
  <xsl:variable name="include-private-members" select="false()"/>

  <xsl:variable name="emphasized-template-parameter-types" select="
    'CompletionToken',
    'ExecutionContext',
    'ExecutionRequest',
    'ExecutionStateType',
    'Executor',
    'FieldViewFwdIterator',
    'Formattable',
    'OutputString',
    'ResultsType',
    'SocketStream',
    'StaticRow',
    'Stream',
    'WritableField',
    'WritableFieldTuple'
  "/>

</xsl:stylesheet>
