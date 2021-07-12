require 'rubygems'
require 'bundler'
require 'bundler/setup'
Bundler.require(:default)

require_relative("CodeTheme")
require_relative("Processors")

Asciidoctor::Extensions.register do 
    docinfo_processor StyleOverrideDocinfoProcessor
end
# @todo: Remove hardcoded path to output, would like it in our output directory
puts "Setting base_dir to %s" % File.join(Dir.pwd, "/..")
Asciidoctor.convert_file(ARGV[0], {to_file: ARGV[1], safe: :unsafe, mkdirs: true, base_dir: File.expand_path(File.join(Dir.pwd, "/..")) })
puts "Documentation generation finished..."
