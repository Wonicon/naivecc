#!/usr/bin/env ruby

filename = "lexical.template"

puts "Open #{filename}"

autos = []
autos_max_width = 0 # Record the max width of the string in the autos
read_flag = false   # Indicate whether to start read lines into autos
lineno = 0          # Record line number
file = open(filename, "r")
file.read.each_line do |line|
    lineno = lineno + 1
    if /READ START/ =~ line
        puts "Reading starts at line #{lineno}"
        read_flag = true
        next
    elsif /READ END/ =~ line
        puts "Reading ends at line #{lineno}"
        read_flag = false
    end

    if read_flag == true
        token = line.split[0]
        autos_max_width = autos_max_width >= token.length ? autos_max_width : token.length
        autos << token
    end
end

file = open(filename, "r")
output = open("../lexical.l", "w")
gen_flag = false
file.read.each_line do |line|
    if /GEN HERE/ =~ line
        autos.each do |token|
            output.write("{#{token}}" + ' ' * (autos_max_width - token.length + 2) + "HANDLE(#{token})\n")
        end
    else
        output.write(line)
    end
end
file.close
output.close
