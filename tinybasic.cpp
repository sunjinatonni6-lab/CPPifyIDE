#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(void, hostPrint, (const char* text), { if (globalThis.cppifyPrint) globalThis.cppifyPrint(UTF8ToString(text)); });
EM_JS(char*, hostInput, (const char* name), {
  const value = globalThis.cppifyInput ? globalThis.cppifyInput(UTF8ToString(name)) : prompt('Enter a value for ' + UTF8ToString(name) + ':');
  return stringToNewUTF8(value == null ? '' : String(value));
});
#else
static void hostPrint(const char* text) { std::cout << text << '\n'; }
static char* hostInput(const char* name) { static std::string value; std::cout << "Enter " << name << ": "; std::getline(std::cin, value); return value.data(); }
#endif

namespace tinybasic {
struct Value {
  bool isString = false;
  double number = 0;
  std::string string;
  Value() = default;
  explicit Value(double n) : number(n) {}
  explicit Value(std::string s) : isString(true), string(std::move(s)) {}
  double numeric() const { if (!isString) return number; char* end=nullptr; double n=std::strtod(string.c_str(), &end); return end==string.c_str() || *end ? 0.0 : n; }
  std::string display() const { if (isString) return string; std::ostringstream out; out.precision(15); out << number; return out.str(); }
};
struct Token { std::string type, text; double number=0; };
struct Statement { std::string type, name; std::vector<Token> expr; int line=0; };
static bool identStart(char c) { return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'; }
static bool identChar(char c) { return identStart(c)||(c>='0'&&c<='9'); }

class Engine {
  std::map<std::string,Value> vars;
  std::vector<Token> tokens;
  size_t pos=0; int line=1;
  [[noreturn]] void fail(const std::string& msg) const { throw std::runtime_error("Line " + std::to_string(line) + ": " + msg); }
  std::vector<Token> lex(const std::string& s) {
    std::vector<Token> out; size_t i=0;
    while(i<s.size()) {
      char c=s[i]; if(c==' '||c=='\t'||c=='\r'){++i;continue;}
      if(c=='"'||c=='\'') { char q=c; ++i; std::string v; bool closed=false;
        while(i<s.size()){char x=s[i++];if(x==q){closed=true;break;}if(x=='\\'){if(i>=s.size())break;char e=s[i++];switch(e){case 'n':v+='\n';break;case 't':v+='\t';break;case 'r':v+='\r';break;default:v+=e;}}else v+=x;}
        if(!closed)fail("Unclosed string literal");out.push_back({"string",v,0});continue;
      }
      if((c>='0'&&c<='9')||(c=='.'&&i+1<s.size()&&s[i+1]>='0'&&s[i+1]<='9')) {size_t b=i;bool dot=false;while(i<s.size()&&((s[i]>='0'&&s[i]<='9')||s[i]=='.')){if(s[i]=='.'&&dot)fail("Invalid number");if(s[i]=='.')dot=true;++i;}std::string raw=s.substr(b,i-b);out.push_back({"number",raw,std::strtod(raw.c_str(),nullptr)});continue;}
      if(identStart(c)){size_t b=i++;while(i<s.size()&&identChar(s[i]))++i;out.push_back({"identifier",s.substr(b,i-b),0});continue;}
      std::string two=i+1<s.size()?s.substr(i,2):"";if(two=="++"||two=="--"||two=="**"){out.push_back({two,two,0});i+=2;continue;}
      if(std::string("+-*/%^()").find(c)!=std::string::npos){out.push_back({std::string(1,c),std::string(1,c),0});++i;continue;}
      fail(std::string("Unexpected character '")+c+"'");
    } return out;
  }
  bool accept(const std::string& t){if(pos<tokens.size()&&tokens[pos].type==t){++pos;return true;}return false;}
  Value primary(){
    if(accept("+"))return Value(primary().numeric());
    if(accept("-"))return Value(-primary().numeric());
    if(accept("(")){Value v=additive();if(!accept(")"))fail("Expected ')'");return v;}
    if(pos>=tokens.size())fail("Expected a value");Token t=tokens[pos++];
    if(t.type=="number")return Value(t.number);if(t.type=="string")return Value(t.text);
    if(t.type=="identifier"){auto it=vars.find(t.text);if(it==vars.end())fail("Variable '"+t.text+"' is not defined");return it->second;}
    fail("Invalid expression");
  }
  Value power(){Value a=primary();if(accept("^")||accept("**")){Value b=power();return Value(std::pow(a.numeric(),b.numeric()));}return a;}
  Value multiply(){Value a=power();while(pos<tokens.size()&&(tokens[pos].type=="*"||tokens[pos].type=="/"||tokens[pos].type=="%")){std::string op=tokens[pos++].type;Value b=power();double x=a.numeric(),y=b.numeric();if((op=="/"||op=="%")&&y==0)fail("Division by zero");a=Value(op=="*"?x*y:op=="/"?x/y:std::fmod(x,y));}return a;}
  Value additive(){Value a=multiply();while(pos<tokens.size()&&(tokens[pos].type=="+"||tokens[pos].type=="-")){std::string op=tokens[pos++].type;Value b=multiply();if(op=="+"&&(a.isString||b.isString))a=Value(a.display()+b.display());else a=Value(op=="+"?a.numeric()+b.numeric():a.numeric()-b.numeric());}return a;}
  Value eval(const std::vector<Token>& ts){tokens=ts;pos=0;if(tokens.empty())fail("Missing expression");Value v=additive();if(pos!=tokens.size())fail("Unexpected token in expression");return v;}
  static std::string stripComment(const std::string& s){char quote=0;bool esc=false;for(size_t i=0;i<s.size();++i){char c=s[i];if(quote){if(esc){esc=false;continue;}if(c=='\\'){esc=true;continue;}if(c==quote)quote=0;continue;}if(c=='\"'||c=='\'')quote=c;else if(c=='#'||(c=='/'&&i+1<s.size()&&s[i+1]=='/'))return s.substr(0,i);}return s;}
  static std::string trim(std::string s){size_t a=s.find_first_not_of(" \t\r");if(a==std::string::npos)return "";size_t b=s.find_last_not_of(" \t\r");return s.substr(a,b-a+1);}
  static bool validName(const std::string& s){if(s.empty()||!identStart(s[0]))return false;for(char c:s)if(!identChar(c))return false;return true;}
  std::vector<Statement> parse(const std::string& source){std::vector<Statement> out;std::istringstream input(source);std::string raw;line=0;
    while(std::getline(input,raw)){++line;std::string s=trim(stripComment(raw));if(s.empty())continue;if(s.back()==';')s=trim(s.substr(0,s.size()-1));if(s.empty())continue;
      auto addExpr=[&](std::string type,std::string name,std::string expr){out.push_back({type,name,lex(expr),line});};
      if(s.rfind("let ",0)==0){size_t eq=s.find('=');if(eq==std::string::npos)fail("Expected '=' in variable declaration");std::string name=trim(s.substr(4,eq-4));if(!validName(name))fail("Invalid variable name");addExpr("let",name,s.substr(eq+1));}
      else if(s.rfind("print ",0)==0)addExpr("print","",s.substr(6));
      else if(s.rfind("input ",0)==0){std::string name=trim(s.substr(6));if(!validName(name))fail("Invalid input variable name");out.push_back({"input",name,{},line});}
      else if(s.size()>2&&(s.substr(s.size()-2)=="++"||s.substr(s.size()-2)=="--")){std::string name=trim(s.substr(0,s.size()-2));if(!validName(name))fail("Invalid increment/decrement target");out.push_back({s.substr(s.size()-2)=="++"?"inc":"dec",name,{},line});}
      else {size_t eq=s.find('=');if(eq==std::string::npos)fail("Unknown statement or invalid syntax");std::string name=trim(s.substr(0,eq));if(!validName(name))fail("Invalid assignment target");addExpr("assign",name,s.substr(eq+1));}
    }return out;
  }
public:
  void run(const std::string& source){vars.clear();try{auto program=parse(source);for(const auto& st:program){line=st.line;if(st.type=="let"||st.type=="assign")vars[st.name]=eval(st.expr);else if(st.type=="print"){std::string v=eval(st.expr).display();hostPrint(v.c_str());}else if(st.type=="input"){char* p=hostInput(st.name.c_str());vars[st.name]=Value(std::string(p?p:""));
#ifdef __EMSCRIPTEN__
 free(p);
#endif
 }else{auto it=vars.find(st.name);if(it==vars.end())fail("Variable '"+st.name+"' is not defined");double n=it->second.numeric();it->second=Value(n+(st.type=="inc"?1:-1));}}
 hostPrint("✓ Program finished successfully.");}catch(const std::exception& e){std::string msg=std::string("✕ ")+e.what();hostPrint(msg.c_str());}}
};
}

#ifdef __EMSCRIPTEN__
extern "C" EMSCRIPTEN_KEEPALIVE void runTinyBasic(const char* source){tinybasic::Engine engine;engine.run(source?source:"");}
#else
int main(){std::ostringstream source;source<<std::cin.rdbuf();tinybasic::Engine engine;engine.run(source.str());}
#endif
