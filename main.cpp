/*
 * Copyright 2015 Ryoko Akizuki<ryokoakizuki@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fstream>
#include <iostream>
#include <string>
#include <set>
#include <map>
#include <vector>
#include <deque>
#include <cctype>
#include <sstream>

std::string getWordClass(const std::string &c)
{
    std::vector<const char*> classes = {
        "n", "noun",
        "pron", "pronoun",
        "v", "verb",
        "adj", "adjective",
        "adv", "adverb",
        "prep", "preposition",
        "conj", "conjunction"
    };
    for(size_t i = 0; i < classes.size(); ++i)
    {
        if(c == classes[i])
        {
            return (i % 2 == 0) ? classes[i + 1] : classes[i];
        }
    }
    return "unknown";
}

struct Word
{
    std::string                             word; // water, sun, ...
    // std::string type; // noun, verb, ...
    std::multimap<std::string, std::string> defi; // <class, def>
    std::set<std::string>                   coll;
    std::set<std::string>                   exam;
    std::set<std::string>                   cate;

            void            print(std::ostream &s)
    {
        // word
        s << word << "\n";
        // defi
        if(defi.empty())
        {
            s << "<no definitions>\n";
        }
        else
        {
            s << "[definitions]\n";
            for(auto i = defi.begin(); i != defi.end(); ++i)
            {
                s << i->first << ": " << i->second << '\n';
            }
        }
        // coll
        if(coll.empty()) s << "<no collocations>\n";
        else { s << "[collocations]\n"; for(auto &c : coll) s << c << "\n"; }
        // exam
        if(exam.empty()) s << "<no examples>\n";
        else { s << "[examples]\n"; for(auto &e : exam) s << e << "\n"; }
        // cate
        if(cate.empty()) s << "<uncategorized>\n";
        else { s << "[categories]\n"; for(auto &c : cate) s << c << "\n"; }
    }

    friend  std::istream&   operator>>(std::istream &stream, Word &w)
    {
        enum state
        {
            seek_word_block,
            seek_word_entity,
            read_word_entity,
            seek_item,
            begin_item_title,
            read_item_title,
            seek_item_end,
            seek_item_content,
            read_item_content,
            block_ended,
            bad_state
        };
        char c;
        state state = seek_word_block;
        std::deque<std::string> word_stack;
        while((c = stream.get()) != std::char_traits<char>::eof() && state != bad_state && state != block_ended)
        {
            switch(state)
            {
                case seek_word_block:
                {
                    if(isblank(c) || c =='\n') break; // ignore spaces and newlines
                    if(c == '[')
                    {
                        state = seek_word_entity;
                        break;
                    }
                    // we shouldn't meet any other character
                    state = bad_state;
                    std::cerr << "expected begin of word block." << std::endl;
                    break;
                }
                case seek_word_entity:
                {
                    if(isblank(c) || c =='\n') break; // ignore spaces and newlines
                    if(isalpha(c))
                    {
                        state = read_word_entity;
                        word_stack.emplace_back(1, c);
                        // std::cerr << "push word entity" << std::endl;
                        break;
                    }
                    state = bad_state;
                    std::cerr << "expected word entity." << std::endl;
                    break;
                }
                case read_word_entity:
                {
                    if(isalpha(c))
                    {
                        word_stack.back().push_back(c);
                    }
                    else
                    {
                        if(word_stack.empty())
                        {
                            state = bad_state;
                            std::cerr << "expected word entity." << std::endl;
                            break;
                        }
                        if(!w.word.empty() && w.word != word_stack.back())
                        {
                            state = bad_state;
                            std::cerr << "trying to merge different words." << std::endl;
                            break;
                        }
                        w.word = std::move(word_stack.back());
                        word_stack.pop_back();
                        // std::cerr << "pop word entity" << std::endl;
                        state = seek_item;
                    }
                    break;
                }
                case seek_item:
                {
                    if(isblank(c) || c =='\n') break; // ignore spaces and newlines
                    if(c == ':')
                    {
                        state = begin_item_title;
                        break;
                    }
                    if(c == ']')
                    {
                        state = block_ended;
                        break;
                    }
                    state = bad_state;
                    std::cerr << "expected item indicator." << std::endl;
                    break;
                }
                case begin_item_title:
                {
                    if(isblank(c) || c =='\n') break; // ignore spaces and newlines
                    if(isalpha(c))
                    {
                        state = read_item_title;
                        word_stack.emplace_back(1, c);
                        // std::cerr << "push item title" << std::endl;
                        break;
                    }
                    state = bad_state;
                    std::cerr << "expected item title." << std::endl;
                    break;
                }
                case read_item_title:
                {
                    if(isalpha(c))
                    {
                        word_stack.back().push_back(c);
                        break;
                    }
                    if(isblank(c) || c =='\n')
                    {
                        state = seek_item_end;
                        break;
                    }
                    if(c == ':')
                    {
                        state = seek_item_content;
                        break;
                    }
                    state = bad_state;
                    std::cerr << "expected item end." << std::endl;
                    break;
                }
                case seek_item_end:
                {
                    if(isblank(c) || c =='\n') break;
                    if(c == ':')
                    {
                        state = seek_item_content;
                        break;
                    }
                    state = bad_state;
                    std::cerr << "expected item end." << std::endl;
                    break;
                }
                case seek_item_content:
                {
                    if(isblank(c) || c =='\n') break;
                    if(c == '.')
                    {
                        state = seek_item_content;
                        std::cerr << "warning: blank item content." << std::endl;
                        break;
                    }
                    if(c == ':')
                    {
                        state = begin_item_title;
                        break;
                    }
                    if(c == ']')
                    {
                        state = block_ended;
                        break;
                    }
                    state = read_item_content;
                    word_stack.emplace_back(1, c);
                    // std::cerr << "push item content" << std::endl;
                    break;
                }
                case read_item_content:
                {
                    if(c =='.')
                    {
                        if(word_stack.empty())
                        {
                            state = bad_state;
                            std::cerr << "expected item content." << std::endl;
                            break;
                        }
                        auto s = word_stack.back();
                        word_stack.pop_back();
                        // std::cerr << "pop item content" << std::endl;
                        if(word_stack.back() == "defi")
                        {
                            size_t bracket_begin = s.find('('), bracket_end = s.find(')');
                            if(bracket_begin == std::string::npos || bracket_end == std::string::npos)
                            {
                                std::cerr << "bracket not properly closed, treat as unknown class." << std::endl;
                                w.defi.insert(std::make_pair("unknown", std::move(s)));
                            }
                            else
                            {
                                if(bracket_begin > bracket_end)
                                {
                                    std::cerr << "wrong bracket order, treat as unknown class." << std::endl;
                                    w.defi.insert(std::make_pair("unknown", std::move(s)));
                                }
                                std::string word_class(s.begin() + bracket_begin + 1, s.begin() + bracket_end);
                                s.erase(s.begin(), s.begin() + bracket_end + 1);
                                w.defi.insert(std::make_pair(getWordClass(word_class), std::move(s)));
                            }
                        }
                        else if(word_stack.back() == "coll")
                            w.coll.insert(std::move(s));
                        else if(word_stack.back() == "exam")
                            w.exam.insert(std::move(s));
                        else if(word_stack.back() == "cate")
                            w.cate.insert(std::move(s));
                        else
                            std::cerr << "unrecognized item, ignored." << std::endl;

                        state = seek_item_content;
                    }
                    else
                    {
                        if(c == ':')
                        {
                            std::cerr << "warning: item content unexpectedly ended with ':'." << std::endl;
                            state = begin_item_title;
                            break;
                        }
                        if(c == ']')
                        {
                            std::cerr << "warning: item content unexpectedly ended with ']'." << std::endl;
                            state = block_ended;
                            break;
                        }
                        if(c != '\n') word_stack.back().push_back(c);
                    }
                    break;
                }
            }
        }
        if(state == bad_state)
        {
            std::cerr << "error parsing word record at position " << stream.tellg() << std::endl;
        }
        return stream;
    }
};

int main()
{
    std::stringstream s;
    s << R"(

[
word
:defi:
gjhjfgkfjlasf.
fasoijidjsaofoa.
foiadjsfoifjsoid
:coll:fdsdssff fdfdfsfdsff.fdsjfdsojd dfdsf5fds9.fdjffjds
]

[
    word
    :  defi:(n)gjhjfgkfjlasf.(adj)fasoijidjsaofoa.foiadjsfoifjsoid.
    :coll:fdsdssff fdffdgfdfdfsfdsff.fdfdfgggggsjfdsojd dfdsf5fds9. fdhkkjffjds.
]
[
    set
    :  defi:gjhjfgkfjlasf.fasoijidjsaofoa.foiadjsfoifjsoid.
    :coll:fdsdssff fdfdfsfdsff. fdsjfdsojd dfdsf5fds9. fdjffjds.
]
)";

    Word w;
    s >> w;
    s >> w;
    s >> w;
    w.print(std::cout);
}
