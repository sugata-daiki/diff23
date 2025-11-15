#include <iostream>
#include <string>
#include <memory>    // std::shared_ptr
#include <format>    // C++20 (C++23でも利用可)
#include <stdexcept> // std::runtime_error
#include <utility>   // std::move

//-------------------------------------------------
// 1. クラス前方宣言
//-------------------------------------------------
struct Expression;
struct Constant;
struct Variable;
struct BinaryOp;
struct Add;
struct Multiply;

//-------------------------------------------------
// 2. 補助関数 (dynamic_cast ラッパー)
//-------------------------------------------------
template<typename T>
const T* as(const Expression* expr) {
    return dynamic_cast<const T*>(expr);
}

//-------------------------------------------------
// 3. クラス「宣言」
//-------------------------------------------------

struct Expression {
    virtual ~Expression() = default;
    virtual double evaluate(double x_val) const = 0;
    virtual std::shared_ptr<Expression> derivative() const = 0;
    virtual std::shared_ptr<Expression> simplify() const = 0;
    virtual std::string to_string() const = 0;
};

struct Constant : Expression {
    double value;
    explicit Constant(double v) : value(v) {}
    double evaluate(double x_val) const override;
    std::shared_ptr<Expression> derivative() const override;
    std::shared_ptr<Expression> simplify() const override;
    std::string to_string() const override;
};

struct Variable : Expression {
    std::string name = "x";
    double evaluate(double x_val) const override;
    std::shared_ptr<Expression> derivative() const override;
    std::shared_ptr<Expression> simplify() const override;
    std::string to_string() const override;
};

struct BinaryOp : Expression {
    std::shared_ptr<Expression> left, right;
    BinaryOp(std::shared_ptr<Expression> l, std::shared_ptr<Expression> r)
        : left(std::move(l)), right(std::move(r)) {}
};

struct Multiply : BinaryOp {
    Multiply(std::shared_ptr<Expression> l, std::shared_ptr<Expression> r)
        : BinaryOp(std::move(l), std::move(r)) {}
    double evaluate(double val) const override;
    std::shared_ptr<Expression> derivative() const override;
    std::shared_ptr<Expression> simplify() const override;
    std::string to_string() const override;
};

struct Add : BinaryOp {
    Add(std::shared_ptr<Expression> l, std::shared_ptr<Expression> r)
        : BinaryOp(std::move(l), std::move(r)) {}
    double evaluate(double val) const override;
    std::shared_ptr<Expression> derivative() const override;
    std::shared_ptr<Expression> simplify() const override;
    std::string to_string() const override;
};

//-------------------------------------------------
// 4. ファクトリ関数 (★ 名前を変更)
//-------------------------------------------------
auto C(double v) { return std::shared_ptr<Constant>(new Constant(v)); }
auto V() { return std::shared_ptr<Variable>(new Variable()); }

// 名前を変更: Add -> make_add
auto make_add(std::shared_ptr<Expression> l, std::shared_ptr<Expression> r) { 
    return std::shared_ptr<Add>(new Add(std::move(l), std::move(r))); 
}
// 名前を変更: Mul -> make_mul
auto make_mul(std::shared_ptr<Expression> l, std::shared_ptr<Expression> r) { 
    return std::shared_ptr<Multiply>(new Multiply(std::move(l), std::move(r))); 
}


//-------------------------------------------------
// 5. クラス「定義」 (実装)
// (実装内部でもファクトリ関数を使用)
//-------------------------------------------------

// --- Constant ---
double Constant::evaluate(double /*x_val*/) const { return value; }
std::shared_ptr<Expression> Constant::derivative() const {
    return C(0);
}
std::shared_ptr<Expression> Constant::simplify() const {
    return C(value);
}
std::string Constant::to_string() const {
    return std::format("{}", value);
}

// --- Variable ---
double Variable::evaluate(double x_val) const {
    return x_val;
}
std::shared_ptr<Expression> Variable::derivative() const {
    return C(1);
}
std::shared_ptr<Expression> Variable::simplify() const {
    return V();
}
std::string Variable::to_string() const {
    return name;
}

// --- Multiply ---
double Multiply::evaluate(double val) const {
    return left->evaluate(val) * right->evaluate(val);
}
std::shared_ptr<Expression> Multiply::derivative() const {
    // ★ make_add, make_mul を使用
    return make_add(
        make_mul(left->derivative(), right),
        make_mul(left, right->derivative())
    );
}
std::shared_ptr<Expression> Multiply::simplify() const {
    auto l = left->simplify();
    auto r = right->simplify();

    auto lc = as<Constant>(l.get());
    auto rc = as<Constant>(r.get());

    if (lc && rc) return C(lc->value * rc->value);
    if (rc && rc->value == 0) return C(0);
    if (lc && lc->value == 0) return C(0);
    if (rc && rc->value == 1) return l;
    if (lc && lc->value == 1) return r;

    return make_mul(l, r); // ★ make_mul を使用
}
std::string Multiply::to_string() const {
    return std::format("({} * {})", left->to_string(), right->to_string());
}

// --- Add ---
double Add::evaluate(double val) const {
    return left->evaluate(val) + right->evaluate(val);
}
std::shared_ptr<Expression> Add::derivative() const {
    return make_add(left->derivative(), right->derivative()); // ★ make_add を使用
}
std::shared_ptr<Expression> Add::simplify() const {
    auto l = left->simplify();
    auto r = right->simplify();

    auto lc = as<Constant>(l.get());
    auto rc = as<Constant>(r.get());

    if (lc && rc) return C(lc->value + rc->value);
    if (rc && rc->value == 0) return l;
    if (lc && lc->value == 0) return r;

    auto l_mul = as<Multiply>(l.get());
    auto r_mul = as<Multiply>(r.get());
    auto l_var = as<Variable>(l.get());
    auto r_var = as<Variable>(r.get());

    // (C1 * x) + (C2 * x)
    if (l_mul && r_mul) {
        auto l_c = as<Constant>(l_mul->left.get());
        auto l_v = as<Variable>(l_mul->right.get());
        auto r_c = as<Constant>(r_mul->left.get());
        auto r_v = as<Variable>(r_mul->right.get());
        if (l_c && l_v && r_c && r_v) {
            return make_mul(C(l_c->value + r_c->value), V()); // ★
        }
    }
    
    // x + (C * x)
    if (l_var && r_mul) {
        auto r_c = as<Constant>(r_mul->left.get());
        auto r_v = as<Variable>(r_mul->right.get());
        if (r_c && r_v) {
            return make_mul(C(1 + r_c->value), V()); // ★
        }
    }
    
    // (C * x) + x
    if (l_mul && r_var) {
        auto l_c = as<Constant>(l_mul->left.get());
        auto l_v = as<Variable>(l_mul->right.get());
        if (l_c && l_v) {
            return make_mul(C(l_c->value + 1), V()); // ★
        }
    }
    
    return make_add(l, r); // ★ make_add を使用
}
std::string Add::to_string() const {
    return std::format("({} + {})", left->to_string(), right->to_string());
}


//-------------------------------------------------
// 6. メイン (実行例)
//-------------------------------------------------
int main() {
    // f(x) = x + 2x
    // (x + (2 * x))
    auto f1 = make_add(V(), make_mul(C(2), V())); // ★ make_add, make_mul

    std::cout << "--- パターン1: (x + 2x) をそのまま微分 -> 簡約化 ---\n";
    std::cout << "f(x) = " << f1->to_string() << "\n";

    auto df1 = f1->derivative();
    std::cout << "f'(x) (微分直後) = " << df1->to_string() << "\n";

    auto df1_simplified = df1->simplify();
    std::cout << "f'(x) (簡約後)   = " << df1_simplified->to_string() << "\n";
    
    std::cout << "\n--- パターン2: (x + 2x) を先に簡約化 -> 微分 ---\n";
    std::cout << "f(x) = " << f1->to_string() << "\n";

    auto f1_simplified = f1->simplify();
    std::cout << "f(x) (簡約後) = " << f1_simplified->to_string() << "\n";
    
    auto df1_pre_simplified = f1_simplified->derivative();
    std::cout << "f'(x) (微分)    = " << df1_pre_simplified->to_string() << "\n";
    
    auto df1_final = df1_pre_simplified->simplify();
    std::cout << "f'(x) (最終簡約) = " << df1_final->to_string() << "\n";

    std::cout << "\n--- 別の例 (g(x) = x*x) ---\n";
    // g(x) = x * x
    auto g = make_mul(V(), V()); // ★ make_mul
    std::cout << "g(x) = " << g->to_string() << "\n";
    
    auto dg = g->derivative();
    std::cout << "g'(x) (微分直後) = " << dg->to_string() << "\n";
    
    auto dg_simplified = dg->simplify();
    std::cout << "g'(x) (簡約後)   = " << dg_simplified->to_string() << "\n";
    
    std::cout << "g'(x) at x=5 (評価): " << dg_simplified->evaluate(5) << "\n";

    return 0;
}
