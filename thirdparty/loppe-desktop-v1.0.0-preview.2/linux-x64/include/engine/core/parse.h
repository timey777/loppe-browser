/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include "attribute.h"
#include "ast_snippet.h"
#include "loppe_export.h"
struct ASTNode;
/*
  简要说明：
  - 可扩展：实现 IParser 接口并在 ParserRegistry 注册即可新增语法块，比如 @event{}、@script{}。
*/
namespace loppe {
    // ------------------- Token & Lazy TokenStream （按需词法，支持多种 LexerMode） -------------------
    enum class TokenType {
        Identifier,
        String,
        Number,
        Symbol,
        Keyword,   // e.g. @style, @layout, @import
        Eof
    };

    struct Token {
        TokenType type;
        std::string text;
        size_t line = 1, col = 1;
        // 本 token 在源码里的起始字节偏移（跳过空白**之后**的位置）。
        // 聚合初始化按此顺序：{ type, text, line, col, offset[, spaceBefore] }。
        size_t offset = 0;
        // 与前一 token 之间是否“确有空白”（选择器拼接用：无空白 = 同元素复合 ".a.b"，有空白 = 后代 ".a .b"）。
        // 由 Style/Layout 词法器（lexNextStyle）填充；其它模式保持默认 false。
        bool spaceBefore = false;
    };

    class TokenStream {
    public:
        enum class LexerMode { Default, Style, Layout };
        explicit TokenStream(const std::string& s);

        // peek lookahead-th token (0=current). Will lex tokens on demand.
        const Token& peek(size_t lookahead = 0);
        // consume and return next token
        Token next();
        // helper to check EOF quickly
        bool eof();

        // 控制词法器模式（栈式，便于嵌套切换）
        void pushLexer(LexerMode m);
        void popLexer();

    private:
        std::string src;
        size_t charPos;
        size_t line, col;
        std::vector<Token> tokens;
        size_t cur;
        static Token eofTok;

        std::vector<LexerMode> modeStack;

        void ensureTokens(size_t need);

        void advanceChar();
        // 跳过空白与注释；返回是否“跳过了空白字符”（注释不算空白——CSS 里注释不产生后代分隔）
        bool skipWhitespace();
        bool peekIsDigit() const;
        bool isSymbolChar(char c) const;

        // 按当前模式分发
        Token lexNext();

        // 各模式词法器声明
        Token lexNextDefault();
        Token lexNextStyle();
        Token lexNextLayout();
    };

    // RAII 辅助，用于在解析器中临时切换词法器并在退出时恢复
    struct LexerGuard {
        TokenStream& ts;
        LexerGuard(TokenStream& s, TokenStream::LexerMode m);
        ~LexerGuard();
    };


    // Visitor 接口
    class LOPPE_API IVisitor {
    public:
        virtual ~IVisitor() = default;
        virtual void visit(std::shared_ptr<ASTNode> node, int depth) = 0;
    };

    // AST 打印实现（Visitor）
    class LOPPE_API ASTPrinter : public IVisitor {
    public:
        void visit(std::shared_ptr<ASTNode> node, int depth) override;
    private:
        void printIndent(int d);
        void printAttr(const Attribute& a);
    };

    // ------------------- Parser 抽象与 Registry -------------------
    class IParser {
    public:
        virtual ~IParser() = default;
        // 判断是否能在当前 TokenStream 的当前位置匹配当前语法块（无状态）
        virtual bool match(TokenStream& ts) const = 0;
        // 解析，从当前流位置开始，返回 ASTNode（会消费相应的 token）
        virtual std::shared_ptr<ASTNode> parse(TokenStream& ts) const = 0;
    };

    class ParserRegistry {
    public:
        static ParserRegistry& instance();
        void registerParser(const std::string& name, std::shared_ptr<IParser> p);
        // 找第一个 match 的 parser（顺序敏感 —— 注册顺序可以控制优先级）
        std::shared_ptr<IParser> findMatching(TokenStream& ts) const;
    private:
        ParserRegistry() = default;
        std::vector<std::shared_ptr<IParser>> parsers;
    };

    // ------------------- 各具体 Parser 声明 -------------------

    // @import("s.lopstyle")
    class ImportParser : public IParser {
    public:
        bool match(TokenStream& ts) const override;
        std::shared_ptr<ASTNode> parse(TokenStream& ts) const override;
    };

    // @style { ... }  // CSS-like简化版本： selector { key: value; ... }
    class StyleParser : public IParser {
    public:
        bool match(TokenStream& ts) const override;
        std::shared_ptr<ASTNode> parse(TokenStream& ts) const override;
    };

    // @layout { ... }  // very similar structure to style
    class LayoutParser : public IParser {
    public:
        bool match(TokenStream& ts) const override;
        std::shared_ptr<ASTNode> parse(TokenStream& ts) const override;
    };

    // ContentParser：解析类似 panel:Main ( ... ) { ... }
    class ContentParser : public IParser {
    public:
        bool match(TokenStream& ts) const override;
        std::shared_ptr<ASTNode> parse(TokenStream& ts) const override;
    private:
        void parseAttrList(TokenStream& ts, std::shared_ptr<ASTNode>& node) const;
        void parseClass(ASTNode& node, const std::string& classStr) const;
    };

    // ------------------- 顶层 LoppeParser -------------------
    class LOPPE_API LoppeParser {
    public:
        AstSnippet parse(const std::string& input) const;

    private:
        std::shared_ptr<ASTNode> parseRoot(const std::string& input) const;
    };
}