<?xml version="1.0" encoding="ISO-8859-1"?>

<xsl:stylesheet
xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
xmlns="http://www.w3.org/TR/REC-html40"
version="2.0">

	<xsl:output method="html"/>

	<!-- This recursively splits the input string by newline characters, inserting a <br/> at each point. -->
	<xsl:template name="repNL">
		<!-- this was in below param element - guess it's giving default value for when the template was matching to text().
			select="."
		-->
		<xsl:param name="pText" />

		<!-- Text up to first newline, or all text if no newline. -->
		<xsl:copy-of select="substring-before(concat($pText, '&#xA;'), '&#xA;')"/>

		<!-- Was there a newline? -->
		<xsl:if test="contains($pText, '&#xA;')">
			<!-- Insert <br/> -->
			<br />
			<!-- Recurse, passing string following the newline. -->
			<xsl:call-template name="repNL">
				<xsl:with-param name="pText" select="substring-after($pText, '&#xA;')"/>
			</xsl:call-template>
		</xsl:if>
	</xsl:template>

	<!-- Match all internal text nodes (element content) -->
	<xsl:template match="text()">
		<!-- Do we have any non-whitespace text at all? If not, output nothing. -->
		<xsl:if test="normalize-space(.)">
			<!-- Strip leading and trailing whitespace before initiating above recursive template. -->
			<xsl:call-template name="repNL">
				<xsl:with-param name="pText" select="replace(., '^\s+|\s+$', '')"/>
			</xsl:call-template>
		</xsl:if>
	</xsl:template>

	<!-- Root template -->
	<xsl:template match="/">
		<html>
			<head>
				<title><xsl:value-of select="/root/shorttitle" /></title>
				<link rel="stylesheet" type="text/css" href="../../css/bpdoc.css" />
			</head>
			<body>
				<div id="content_container">
					<xsl:apply-templates/>
				</div>
			</body>
		</html>
	</xsl:template>

	<!-- Templates to match specific elements in the input xml -->
	<xsl:template match="shorttitle">
		<h1 class="title_style">
			<xsl:apply-templates/>
		</h1>
	</xsl:template>

	<xsl:template match="category">
	</xsl:template>

	<xsl:template match="description">
		<p>
			<xsl:apply-templates/>
		</p>
	</xsl:template>

	<xsl:template match="imgpath">
		<img>
			<xsl:attribute name="src">
				<xsl:apply-templates/>
			</xsl:attribute>
		</img>
	</xsl:template>

	<xsl:template match="param">
		<tr>
			<td>
				<div class="param_name title_style">
					<xsl:apply-templates select="name"/>
				</div>
				<div class="param_type">
					<xsl:apply-templates select="type"/>
				</div>
			</td>
			<td>
				<xsl:apply-templates select="description"/>
			</td>
		</tr>
	</xsl:template>

	<!-- This is a named template that is reused by both the Inputs and Outputs sections. -->
	<xsl:template name="parameters">
		<table>
			<!-- @TODO: This should be done in the css! -->
			<colgroup>
				<col width="25%" />
				<col width="75%" />
			</colgroup>
			<tbody>
				<xsl:apply-templates/>
			</tbody>
		</table>
	</xsl:template>

	<xsl:template match="inputs">
		<h3 class="title_style">Inputs</h3>
		<xsl:call-template name="parameters" />
	</xsl:template>

	<xsl:template match="outputs">
		<h3 class="title_style">Outputs</h3>
		<xsl:call-template name="parameters" />
	</xsl:template>

	<xsl:template match="root">
		<a class="navbar_style">
			<xsl:attribute name="href">../../index.html</xsl:attribute>
			<xsl:value-of select="docs_name" />
		</a>
		<a class="navbar_style">&gt;</a>
		<a class="navbar_style">
			<xsl:attribute name="href">../<xsl:value-of select="class_id" />.html</xsl:attribute>
			<xsl:value-of select="class_name" />
		</a>
		<a class="navbar_style">&gt;</a>
		<a class="navbar_style"><xsl:value-of select="shorttitle" /></a>
		
		<xsl:apply-templates />
	</xsl:template>

	<!-- Unwanted elements (can use "a | b | c") -->
	<xsl:template match="fulltitle | docs_name | class_id | class_name"/>

</xsl:stylesheet>
