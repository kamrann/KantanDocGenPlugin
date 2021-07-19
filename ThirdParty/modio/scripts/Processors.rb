

#todo: formatter that wraps at an argument and indents the wrapped portion to the first argument along
#wrap source blocks at 80 col
class StyleOverrideDocinfoProcessor < Asciidoctor::Extensions::DocinfoProcessor
    use_dsl
    at_location :head
    def process document
        %(<style>
body,#toc.toc2 {

    background          : #2d2d3f;
    color               : #f5f5f5;
    font-family         : -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif, "Apple Color Emoji", "Segoe UI Emoji", "Segoe UI Symbol";

}
#toc.toc2 {

    width               :24em;
}
#header,#content,#footnotes,#footer {

    width               : calc(100% - 5em);
    max-width           : initial;
}
@media screen and (min-width: 1280px){

    body.toc2 {

        padding-left    : 24em;
    }
}
#header>h1:first-child {
    
    color               : #44bfd5;
    font-weight         : 500;
}
#toctitle,h1,h2,h3,h4,h5,h6 {

    color               : #44bfd5;
    font-weight         : 500;
}
div.title {

    color               : #44bfd5;
}
#content a {
    
    color               : #44bfd5;
}
#toc a {

    color               : #f5f5f5;
}
#toc ul {
    
    font-family         : -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif, "Apple Color Emoji", "Segoe UI Emoji", "Segoe UI Symbol";
}
.variablename {
    font-family         :"Droid Sans Mono","DejaVu Sans Mono",monospace;
}

.variabletype {

    font-family         :"Droid Sans Mono","DejaVu Sans Mono",monospace;
}

.paramtype {
    font-family         :"Droid Sans Mono","DejaVu Sans Mono",monospace;
}
.paramname {
    font-family         :"Droid Sans Mono","DejaVu Sans Mono",monospace;
}
table {

    background          : #2d2d3f;
}
table tr td {

    color               : #f5f5f5;
}
table thead tr th {

    color               : #f5f5f5;
}
table.stripes-odd .tableblock {

    border              : none;
}
table.stripes-odd tr:nth-of-type(odd) {
    
    background          :#242433;
}
.sect1+.sect1 {
    border-top          :none;
}
.admonitionblock>table td.content {

    color               : #f5f5f5;
}
.admonitionblock>table td.icon .title
{
    color               : #f5f5f5;
}
:not(pre):not([class^=L])>code {

    background          :#242433;
    color               :#f92672;
}

.ulist>.title {

    color               : #44bfd5;
}
</style>)
    end
end