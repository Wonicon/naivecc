#!/usr/bin/env ruby

Dir.glob('./test/*.cmm').sort.each do |item|
    puts "Analyze file #{item}"
    system("./parser #{item}")
    puts "===================================="
end
