<!--
    Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
   
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
-->

<xsl:stylesheet version="3.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xs="http://www.w3.org/2001/XMLSchema"
  exclude-result-prefixes="xs">

  <xsl:variable name="emphasized-template-parameter-types" select="
    'CompletionToken',
    'Stream',
    'Executor',
    'ValueCollection',
    'ValueForwardIterator'
  "/>

</xsl:stylesheet>
