<?xml version="1.0" encoding="ISO-8859-1"?>

<xsl:stylesheet
xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
xmlns="http://www.w3.org/TR/REC-html40"
version="2.0">

	<xsl:output method="html"/>

	<!-- Root template -->
	<xsl:template match="/">
		<html>
			<head>
				<title><xsl:value-of select="/root/display_name" /></title>
				<link rel="stylesheet" type="text/css" href="../css/bpdoc.css" />
			</head>
			<body>
				<div id="content_container">
					<xsl:apply-templates select="/root" />
				</div>
			</body>
		</html>
	</xsl:template>
	
	<xsl:template match="/root">
		<a class="navbar_style">
			<xsl:attribute name="href">../index.html</xsl:attribute>
			<xsl:value-of select="docs_name" />
		</a>
		<a class="navbar_style">&gt;</a>
		<a class="navbar_style"><xsl:value-of select="display_name" /></a>
		<h1 class="title_style"><xsl:value-of select="display_name" /></h1>
		
		<xsl:apply-templates select="nodes" />
	</xsl:template>

	<!-- Templates to match specific elements in the input xml -->
	<xsl:template match="nodes">
		<h2 class="title_style">Nodes</h2>
		<table>
			<tbody>
				<xsl:apply-templates select="node">
					<xsl:sort select="shorttitle"/>
				</xsl:apply-templates>
			</tbody>
		</table>	
	</xsl:template>

	<xsl:template match="node">
		<tr>
			<td>
				<a>
					<xsl:attribute name="href">./nodes/<xsl:value-of select="id" />.html</xsl:attribute>
					<xsl:apply-templates select="shorttitle" />	
				</a>
			</td>
		</tr>
	</xsl:template>

</xsl:stylesheet>
