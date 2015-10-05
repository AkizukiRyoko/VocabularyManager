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
#include <termios.h>
#include <unistd.h>
#include <signal.h>

enum ConsoleColorCode
{
    FG_BLACK    = 30,
    FG_RED      = 31,
    FG_GREEN    = 32,
    FG_YELLOW   = 33,
    FG_BLUE     = 34,
    FG_MAGENTA  = 35,
    FG_CYAN     = 36,
    FG_WHITE    = 37,
    FG_DEFAULT  = 39,
    BG_BLACK    = 40,
    BG_RED      = 41,
    BG_GREEN    = 42,
    BG_YELLOW   = 43,
    BG_BLUE     = 44,
    BG_MAGENTA  = 45,
    BG_CYAN     = 46,
    BG_WHITE    = 47,
    BG_DEFAULT  = 49
};

class ConsoleColorModifier
{
protected:
    ConsoleColorCode    mCode;
    typedef ConsoleColorModifier ThisType;

public:
    ConsoleColorModifier(ConsoleColorCode code) : mCode(code) {}
    virtual ~ConsoleColorModifier() {}

    friend std::ostream& operator<<(std::ostream& os, const ThisType& mod)
    {
        return os << "\033[" << mod.mCode << "m";
    }
};

ConsoleColorModifier
    FRONT_BLACK(FG_BLACK),
    FRONT_RED(FG_RED),
    FRONT_GREEN(FG_GREEN),
    FRONT_YELLOW(FG_YELLOW),
    FRONT_BLUE(FG_BLUE),
    FRONT_MAGENTA(FG_MAGENTA),
    FRONT_CYAN(FG_CYAN),
    FRONT_WHITE(FG_WHITE),
    FRONT_DEFAULT(FG_DEFAULT)
;

ConsoleColorModifier
    BACK_BLACK(BG_BLACK),
    BACK_RED(BG_RED),
    BACK_GREEN(BG_GREEN),
    BACK_YELLOW(BG_YELLOW),
    BACK_BLUE(BG_BLUE),
    BACK_MAGENTA(BG_MAGENTA),
    BACK_CYAN(BG_CYAN),
    BACK_WHITE(BG_WHITE),
    BACK_DEFAULT(BG_DEFAULT)
;

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
    std::multimap<std::string, std::string> defi; // <class, def>
    std::set<std::string>                   coll;
    std::set<std::string>                   exam;
    std::set<std::string>                   cate;

            void            print(std::ostream &s)
    {
        // word
        s << FRONT_CYAN << word << FRONT_DEFAULT << "\n";
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

            void        merge(Word &w)
    {
        if(w.word != word)
        {
            std::cerr << "trying to merge different words." << std::endl;
            return;
        }
        defi.insert(w.defi.begin(), w.defi.end());
        coll.insert(w.coll.begin(), w.coll.end());
        exam.insert(w.exam.begin(), w.exam.end());
        cate.insert(w.cate.begin(), w.cate.end());
    }

    friend  std::ostream&   operator<<(std::ostream &s, const Word &w)
    {
        // word
        s << "[\n" << w.word << "\n";
        // defi
        if(!w.defi.empty())
        {
            s << ":defi:\n";
            for(auto i = w.defi.begin(); i != w.defi.end(); ++i)
            {
                s << "(" << i->first << ")" << i->second << ".\n";
            }
        }
        // coll
        if(!w.coll.empty())
        {
            s << ":coll:\n";
            for(auto &c : w.coll) s << c << ".\n";
        }
        // exam
        if(!w.exam.empty())
        {
            s << ":exam:\n";
            for(auto &c : w.exam) s << c << ".\n";
        }
        // cate
        if(!w.cate.empty())
        {
            s << ":cate:\n";
            for(auto &c : w.cate) s << c << ".\n";
        }
        s << "]" << std::endl;
    }

    friend  std::istream&   operator>>(std::istream &stream, Word &w)
    {
        enum stream_read_state
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
        stream_read_state state = seek_word_block;
        std::deque<std::string> word_stack;
        while(state != bad_state && state != block_ended && (c = stream.get()) != std::char_traits<char>::eof())
        {
            switch(state)
            {
                case seek_word_block:
                {
                    if(isspace(c)) break;
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
                    if(isspace(c)) break;
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
                        // maybe seekg(tellg() - 1)?
                        if(c == ':') state = begin_item_title;
                        else if(c == ']') state = block_ended;
                        else state = seek_item;
                    }
                    break;
                }
                case seek_item:
                {
                    if(isspace(c)) break;
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
                    if(isspace(c)) break;
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
                    if(isspace(c))
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
                    if(isspace(c)) break;
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
                    if(isspace(c)) break;
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
                                // std::cerr << "bracket not properly closed or not found, treat as unknown class." << std::endl;
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

struct termios original_state;

void enableNoncanonicalInput()
{
    struct termios tattr;
    tcgetattr(STDIN_FILENO, &tattr);
    tattr.c_lflag &= ~(ICANON|ECHO); /* Clear ICANON and ECHO. */
    tattr.c_cc[VMIN] = 1;
    tattr.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);  
}

void disableNoncanonicalInput()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_state);
}

void signalHandler(int /* signum */)
{
    std::cerr << "Received signal SIGINT, type '!' to exit." << std::endl;
}

int main()
{
    signal(SIGINT, signalHandler);

    std::map<std::string, Word> word_map;
    std::fstream file;

    file.open("dict", std::ios_base::in);
    Word wcache;
    while(file >> wcache)
    {
        auto &w = word_map[wcache.word];
        w.word = wcache.word;
        w.merge(wcache);
        wcache = Word();
    }
    file.close();
    
    if(!isatty(STDIN_FILENO))
    {
        std::cerr << "STDIN_FILENO is not a terminal." << std::endl;
    }
    tcgetattr(STDIN_FILENO, &original_state); // get current state;
    enableNoncanonicalInput();
    atexit(disableNoncanonicalInput);

    char c;
    enum input_parse_state
    {
        wait_input,
        read_lookup_word,
        add_content,
        bad_state
    };
    std::deque<std::string> word_stack;
    std::deque<input_parse_state> state_stack;
    state_stack.push_back(wait_input);
    Word *cur = nullptr;
    while((c = getchar()) != '!' && c != EOF)
    {
    begin_loop:
        if(state_stack.empty())
        {
            std::cerr << "empty state stack." << std::endl;
            continue;
        }
        switch(state_stack.back())
        {
            case wait_input:
            {
                if(isalpha(c))
                {
                    word_stack.emplace_back();
                    state_stack.emplace_back(read_lookup_word);
                    std::cerr << "lookup: ";
                    goto begin_loop; // let other section handle this char
                }
                if(c == '+')
                {
                    word_stack.emplace_back();
                    state_stack.emplace_back(add_content);
                    std::cerr << "add: ";
                    break;
                }
                break;
            }
            case read_lookup_word:
            {
                if(word_stack.empty())
                {
                    std::cerr << "empty word stack." << std::endl;
                    break;
                }
                if(c == '\n')
                {
                    putchar('\n');
                    if(!word_stack.back().empty())
                    {
                        auto i = word_map.find(word_stack.back());
                        if(i == word_map.end())
                        {
                            std::cerr << "word '" << FRONT_CYAN << word_stack.back() << FRONT_DEFAULT << "' not found." << std::endl;
                            auto lb = word_map.lower_bound(word_stack.back());
                            size_t count = 0;
                            for(; lb != word_map.end() && lb->first.find(word_stack.back()) == 0; ++lb, ++count)
                            {
                                std::cerr << "are you finding '" << FRONT_CYAN << lb->first << FRONT_DEFAULT << "'?" << std::endl;
                            }
                            if(count == 1)
                            {
                                std::cerr << "selecting '" << FRONT_CYAN << (--lb)->first << FRONT_DEFAULT << "'." << std::endl;
                                lb->second.print(std::cout);
                            }
                        }
                        else
                        {
                            i->second.print(std::cout);
                        }
                    }
                    word_stack.pop_back();
                    state_stack.pop_back();
                    break;
                }
                else if(c == 127 || c == '\b')
                {
                    if(!word_stack.empty() && !word_stack.back().empty())
                    {
                        std::cout << "\b \b";
                        word_stack.back().pop_back();
                    }
                    break;
                }
                if(!isalpha(c)) break;
                std::cout << FRONT_CYAN << c << FRONT_DEFAULT;
                word_stack.back().push_back(c);
                break;
            }
            // todo: this section assumes that there are no errors in input
            case add_content:
            {
                if(word_stack.empty())
                {
                    std::cerr << "empty word stack." << std::endl;
                    break;
                }
                // todo: multiple categories
                static std::string head_word, word_class, definition, collocation, category;
                enum add_state
                {
                    read_sentence,
                    read_head_word,
                    read_definition,
                    read_collocation,
                    read_category,
                    read_class,
                    bad_state
                };
                static add_state as = read_sentence;
                switch(as)
                {
                    case read_sentence:
                    {
                        if(isalpha(c))
                        {
                            word_stack.back().push_back(c);
                            std::cout << FRONT_GREEN << c << FRONT_DEFAULT;
                            break;
                        }
                        else if(c == ' ')
                        {
                            if(!word_stack.back().empty() && word_stack.back().back() != ' ')
                            {
                                putchar(' ');
                                word_stack.back().push_back(' ');
                            }
                            break;
                        }
                        else if(c == '{')
                        {
                            as = read_collocation;
                            putchar('{');
                            break;
                        }
                        else if(c == '[')
                        {
                            as = read_head_word;
                            putchar('[');
                            break;
                        }
                        else if(c == 127 || c == '\b')
                        {
                            if(!word_stack.empty() && !word_stack.back().empty())
                            {
                                std::cout << "\b \b";
                                char b = word_stack.back().back();
                                // rollback reading state
                                if(b == '}') as = read_collocation;
                                else if(b == ']') as = read_head_word;
                                word_stack.back().pop_back();
                            }
                            break;
                        }
                        break;
                    }
                    case read_head_word:
                    {
                        if(isalpha(c))
                        {
                            word_stack.back().push_back(c);
                            head_word.push_back(c);
                            std::cout << FRONT_CYAN << c << FRONT_DEFAULT;
                            break;
                        }
                        else if(c == '\'' && head_word.empty())
                        {
                            as = read_category;
                            putchar('\'');
                            break;
                        }
                        else if(c == ']')
                        {
                            as = collocation.empty() ? read_sentence : read_collocation;
                            putchar(']');
                            break;
                        }
                        else if(c == 127 || c == '\b')
                        {
                            if(!word_stack.empty() && !word_stack.back().empty())
                            {
                                std::cout << "\b \b";
                                char b = word_stack.back().back();
                                // rollback reading state
                                if(b == '\'') as = read_category;
                                word_stack.back().pop_back();
                            }
                            break;
                        }
                        break;
                    }
                }
                if(c == '\n')
                {
                    if(head_word.empty())
                    {
                        std::cerr << "no head word specified." << std::endl;
                    }
                    else
                    {
                        auto &w = word_map[head_word];
                        w.word = head_word;
                        std::cout << "editing word '" << FRONT_CYAN << head_word << FRONT_DEFAULT << "'." << std::endl;
                        if(!definition.empty())
                        {
                            auto wcls = getWordClass(word_class);
                            w.defi.insert(std::make_pair(wcls, definition));
                            std::cout << "definition added: (" << FRONT_YELLOW << wcls << FRONT_DEFAULT << ")"
                                << FRONT_WHITE << definition << FRONT_DEFAULT << std::endl;
                        }
                        if(!collocation.empty())
                        {
                            w.coll.insert(collocation);
                            std::cout << "collocation added: " << FRONT_MAGENTA << collocation << FRONT_DEFAULT << std::endl;
                        }
                        if(!category.empty())
                        {
                            w.cate.insert(category);
                            std::cout << "category added: " << FRONT_RED << category << FRONT_DEFAULT << std::endl;
                        }
                    }
                    as = read_sentence;
                    head_word.clear();
                    word_class.clear();
                    definition.clear();
                    collocation.clear();
                    word_stack.pop_back();
                    state_stack.pop_back();
                }
            }
        }
    }

    disableNoncanonicalInput();

    size_t i = 0;
    char name[128] = "dict.old";
    while(std::ifstream(name))
    {
        sprintf(name, "dict.old.%zd", i);
        ++i;
    }
    rename("dict", name);
    file.open("dict", std::ios_base::out);
    for(auto i = word_map.begin(); i != word_map.end(); ++i)
    {
        file << i->second;
    }
    file.close();
}
