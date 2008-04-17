#include "solvespace.h"

Expr *Expr::FromParam(hParam p) {
    Expr *r = AllocExpr();
    r->op = PARAM;
    r->x.parh = p;
    return r;
}

Expr *Expr::FromConstant(double v) {
    Expr *r = AllocExpr();
    r->op = CONSTANT;
    r->x.v = v;
    return r;
}

Expr *Expr::AnyOp(int newOp, Expr *b) {
    Expr *r = AllocExpr();
    r->op = newOp;
    r->a = this;
    r->b = b;
    return r;
}

double Expr::Eval(void) {
    switch(op) {
        case PARAM:         return SS.GetParam(x.parh)->val;
        case PARAM_PTR:     return (x.parp)->val;

        case CONSTANT:      return x.v;

        case PLUS:          return a->Eval() + b->Eval();
        case MINUS:         return a->Eval() - b->Eval();
        case TIMES:         return a->Eval() * b->Eval();
        case DIV:           return a->Eval() / b->Eval();

        case NEGATE:        return -(a->Eval());
        case SQRT:          return sqrt(a->Eval());
        case SQUARE:        { double r = a->Eval(); return r*r; }
        case SIN:           return sin(a->Eval());
        case COS:           return cos(a->Eval());

        default: oops();
    }
}

Expr *Expr::PartialWrt(hParam p) {
    Expr *da, *db;

    switch(op) {
        case PARAM_PTR: oops();
        case PARAM:     return FromConstant(p.v == x.parh.v ? 1 : 0);

        case CONSTANT:  return FromConstant(0);

        case PLUS:      return (a->PartialWrt(p))->Plus(b->PartialWrt(p));
        case MINUS:     return (a->PartialWrt(p))->Minus(b->PartialWrt(p));

        case TIMES:
            da = a->PartialWrt(p);
            db = b->PartialWrt(p);
            return (a->Times(db))->Plus(b->Times(da));

        case DIV:           
            da = a->PartialWrt(p);
            db = b->PartialWrt(p);
            return ((da->Times(b))->Minus(a->Times(db)))->Div(b->Square());

        case SQRT:
            return (FromConstant(0.5)->Div(a->Sqrt()))->Times(a->PartialWrt(p));

        case SQUARE:
            return (FromConstant(2.0)->Times(a))->Times(a->PartialWrt(p));

        case NEGATE:    return (a->PartialWrt(p))->Negate();
        case SIN:       return (a->Cos())->Times(a->PartialWrt(p));
        case COS:       return ((a->Sin())->Times(a->PartialWrt(p)))->Negate();

        default: oops();
    }
}

static char StringBuffer[4096];
void Expr::App(char *s, ...) {
    va_list f;
    va_start(f, s);
    vsprintf(StringBuffer+strlen(StringBuffer), s, f);
}
char *Expr::Print(void) {
    StringBuffer[0] = '\0';
    PrintW();
    return StringBuffer;
}

void Expr::PrintW(void) {
    char c;
    switch(op) {
        case PARAM:     App("param(%08x)", x.parh.v); break;
        case PARAM_PTR: App("param(p%08x)", x.parp->h.v); break;

        case CONSTANT:  App("%.3f", x.v); break;

        case PLUS:      c = '+'; goto p;
        case MINUS:     c = '-'; goto p;
        case TIMES:     c = '*'; goto p;
        case DIV:       c = '/'; goto p;
p:
            App("(");
            a->PrintW();
            App(" %c ", c);
            b->PrintW();
            App(")");
            break;

        case NEGATE:    App("(- "); a->PrintW(); App(")"); break;
        case SQRT:      App("(sqrt "); a->PrintW(); App(")"); break;
        case SQUARE:    App("(square "); a->PrintW(); App(")"); break;
        case SIN:       App("(sin "); a->PrintW(); App(")"); break;
        case COS:       App("(cos "); a->PrintW(); App(")"); break;

        default: oops();
    }
}

#define MAX_UNPARSED 1024
static Expr *Unparsed[MAX_UNPARSED];
static int UnparsedCnt, UnparsedP;

static Expr *Operands[MAX_UNPARSED];
static int OperandsP;

static Expr *Operators[MAX_UNPARSED];
static int OperatorsP;

void Expr::PushOperator(Expr *e) {
    if(OperatorsP >= MAX_UNPARSED) throw "operator stack full!";
    Operators[OperatorsP++] = e;
}
Expr *Expr::TopOperator(void) {
    if(OperatorsP <= 0) throw "operator stack empty (get top)";
    return Operators[OperatorsP-1];
}
Expr *Expr::PopOperator(void) {
    if(OperatorsP <= 0) throw "operator stack empty (pop)";
    return Operators[--OperatorsP];
}
void Expr::PushOperand(Expr *e) {
    if(OperandsP >= MAX_UNPARSED) throw "operand stack full";
    Operands[OperandsP++] = e;
}
Expr *Expr::PopOperand(void) {
    if(OperandsP <= 0) throw "operand stack empty";
    return Operands[--OperandsP];
}
Expr *Expr::Next(void) {
    if(UnparsedP >= UnparsedCnt) return NULL;
    return Unparsed[UnparsedP];
}
void Expr::Consume(void) {
    if(UnparsedP >= UnparsedCnt) throw "no token to consume";
    UnparsedP++;
}

int Expr::Precedence(Expr *e) {
    if(e->op == ALL_RESOLVED) return -1; // never want to reduce this marker
    if(e->op != BINARY_OP && e->op != UNARY_OP) oops();

    switch(e->x.c) {
        case 's':
        case 'n':   return 30;

        case '*':
        case '/':   return 20;

        case '+':
        case '-':   return 10;

        default: oops();
    }
}

void Expr::Reduce(void) {
    Expr *a, *b;

    Expr *op = PopOperator();
    Expr *n;
    int o;
    switch(op->x.c) {
        case '+': o = PLUS;  goto c;
        case '-': o = MINUS; goto c;
        case '*': o = TIMES; goto c;
        case '/': o = DIV;   goto c;
c:
            b = PopOperand();
            a = PopOperand();
            n = a->AnyOp(o, b);
            break;

        case 'n': n = PopOperand()->Negate(); break;
        case 's': n = PopOperand()->Sqrt(); break;

        default: oops();
    }
    PushOperand(n);
}

void Expr::ReduceAndPush(Expr *n) {
    while(Precedence(n) <= Precedence(TopOperator())) {
        Reduce();
    }
    PushOperator(n);
}

void Expr::Parse(void) {
    Expr *e = AllocExpr();
    e->op = ALL_RESOLVED;
    PushOperator(e);

    for(;;) {
        Expr *n = Next();
        if(!n) throw "end of expression unexpected";
        
        if(n->op == CONSTANT) {
            PushOperand(n);
            Consume();
        } else if(n->op == PAREN && n->x.c == '(') {
            Consume();
            Parse();
            n = Next();
            if(n->op != PAREN || n->x.c != ')') throw "expected: )";
            Consume();
        } else if(n->op == UNARY_OP) {
            PushOperator(n);
            Consume();
            continue;
        } else if(n->op == BINARY_OP && n->x.c == '-') {
            // The minus sign is special, because it might be binary or
            // unary, depending on context.
            n->op = UNARY_OP;
            n->x.c = 'n';
            PushOperator(n);
            Consume();
            continue;
        } else {
            throw "expected expression";
        }

        n = Next();
        if(n && n->op == BINARY_OP) {
            ReduceAndPush(n);
            Consume();
        } else {
            break;
        }
    }

    while(TopOperator()->op != ALL_RESOLVED) {
        Reduce();
    }
    PopOperator(); // discard the ALL_RESOLVED marker
}

void Expr::Lex(char *in) {
    while(*in) {
        if(UnparsedCnt >= MAX_UNPARSED) throw "too long";

        char c = *in;
        if(isdigit(c) || c == '.') {
            // A number literal
            char number[70];
            int len = 0;
            while((isdigit(*in) || *in == '.') && len < 30) {
                number[len++] = *in;
                in++;
            }
            number[len++] = '\0';
            Expr *e = AllocExpr();
            e->op = CONSTANT;
            e->x.v = atof(number);
            Unparsed[UnparsedCnt++] = e;
        } else if(isalpha(c) || c == '_') {
            char name[70];
            int len = 0;
            while(isforname(*in) && len < 30) {
                name[len++] = *in;
                in++;
            }
            name[len++] = '\0';

            Expr *e = AllocExpr();
            if(strcmp(name, "sqrt")==0) {
                e->op = UNARY_OP;
                e->x.c = 's';
            } else {
                throw "unknown name";
            }
            Unparsed[UnparsedCnt++] = e;
        } else if(strchr("+-*/()", c)) {
            Expr *e = AllocExpr();
            e->op = (c == '(' || c == ')') ? PAREN : BINARY_OP;
            e->x.c = c;
            Unparsed[UnparsedCnt++] = e;
            in++;
        } else if(isspace(c)) {
            // Ignore whitespace
            in++;
        } else {
            // This is a lex error.
            throw "unexpected characters";
        }
    }
}

Expr *Expr::FromString(char *in) {
    UnparsedCnt = 0;
    OperandsP = 0;
    OperatorsP = 0;

    Expr *r;
    try {
        Lex(in);
        Parse();
        r = PopOperand();
    } catch (char *e) {
        dbp("exception: parse/lex error: %s", e);
        return NULL;
    }
    return r;
}

