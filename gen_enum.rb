#!/usr/bin/env ruby

file = open("syntax.y", "r")

lineno = 0

token_flag = false
nt_flag = false

tokens = []
nonterminals = []

max_len = 0

file.read.each_line do |line|
    lineno = lineno + 1
    if /token start/ =~ line
        puts "Start getting token name at line #{lineno}"
        token_flag = true
        next
    elsif /token end/ =~ line
        puts "Finish getting token name at line #{lineno}"
        token_flag = false
        next
    elsif /nonterminal start/ =~ line
        puts "Start getting nonterminal name at line #{lineno}"
        nt_flag = true
        next
    elsif /nonterminal end/ =~ line
        puts "Finish getting token name at line #{lineno}"
        nt_flag = false
        next
    end

    if token_flag
        words = line.split
        words.each do |word|
            puts "Add token #{word}"
            tokens << word
            max_len = max_len >= word.length ? max_len : word.length
        end
    end

    if nt_flag && /\w+ :/ =~ line
        nt = line.split[0]
        puts "Add non-terminal #{nt}"
        nonterminals << nt
        max_len = max_len >= nt.length ? max_len : nt.length
    end
end

# Generate C enum header

file = open("yytname.h", "w")

start_num = 3 # jump $end error $undefined
file.write("\#ifndef __YYTNAME_INDEX_H__\n")
file.write("\#define __YYTNAME_INDEX_H__\n")

file.write("\nenum YYTNAME_INDEX {\n")

tokens.each do |token|
    file.write(' ' * 4 +  "YY_#{token}" + ' ' * (max_len - token.length + 1) + "= #{start_num},\n")
    start_num = start_num + 1
end

start_num = start_num + 1 # jump $accept

nonterminals.each do |nt|
    file.write(' ' * 4 + "YY_#{nt}" + ' ' * (max_len - nt.length + 1) + "= #{start_num},\n")
    start_num = start_num + 1
end

file.write("};\n")
file.write("\n\#endif\n")
