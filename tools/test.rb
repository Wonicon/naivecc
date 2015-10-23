#!/usr/bin/env ruby

dir = ARGV[0]
path = File.join('.', dir, '*.cmm')
puts "Working on #{path}"
Dir.glob(path).sort.each do |item|
    puts "Analyze file #{item}"
    system("./parser -d #{item}")
    puts "=" * 80
end
