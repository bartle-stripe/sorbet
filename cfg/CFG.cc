#include "CFG.h"
#include <algorithm>
#include <sstream>

// helps debugging
template class std::unique_ptr<ruby_typer::cfg::CFG>;
template class std::unique_ptr<ruby_typer::cfg::BasicBlock>;
template class std::unique_ptr<ruby_typer::cfg::Instruction>;

using namespace std;

namespace ruby_typer {
namespace cfg {

int CFG::FORWARD_TOPO_SORT_VISITED = 1 << 0;
int CFG::BACKWARD_TOPO_SORT_VISITED = 1 << 1;
int CFG::LOOP_HEADER = 1 << 2;

BasicBlock *CFG::freshBlock(int outerLoops, core::Loc loc, BasicBlock *from) {
    if (from != nullptr && from == deadBlock()) {
        return from;
    }
    int id = this->maxBasicBlockId++;
    this->basicBlocks.emplace_back(new BasicBlock());
    BasicBlock *r = this->basicBlocks.back().get();
    r->id = id;
    r->loc = loc;
    r->outerLoops = outerLoops;
    return r;
}

CFG::CFG() {
    freshBlock(0, core::Loc::none(0), nullptr); // entry;
    freshBlock(0, core::Loc::none(0), nullptr); // dead code;
    deadBlock()->bexit.elseb = deadBlock();
    deadBlock()->bexit.thenb = deadBlock();
    deadBlock()->bexit.cond = core::NameRef(0);
}

string CFG::toString(core::Context ctx) {
    stringstream buf;
    string symbolName = this->symbol.info(ctx).fullName(ctx);
    buf << "subgraph \"cluster_" << symbolName << "\" {" << endl;
    buf << "    label = \"" << symbolName << "\";" << endl;
    buf << "    color = blue;" << endl;
    buf << "    \"bb" << symbolName << "_0\" [shape = invhouse];" << endl;
    buf << "    \"bb" << symbolName << "_1\" [shape = parallelogram];" << endl << endl;
    for (int i = 0; i < this->basicBlocks.size(); i++) {
        auto text = this->basicBlocks[i]->toString(ctx);
        buf << "    \"bb" << symbolName << "_" << this->basicBlocks[i]->id << "\" [label = \"" << text << "\"];" << endl
            << endl;
        buf << "    \"bb" << symbolName << "_" << this->basicBlocks[i]->id << "\" -> \"bb" << symbolName << "_"
            << this->basicBlocks[i]->bexit.thenb->id << "\" [style=\"bold\"];" << endl;
        if (this->basicBlocks[i]->bexit.thenb != this->basicBlocks[i]->bexit.elseb) {
            buf << "    \"bb" << symbolName << "_" << this->basicBlocks[i]->id << "\" -> \"bb" << symbolName << "_"
                << this->basicBlocks[i]->bexit.elseb->id << "\" [style=\"tapered\"];" << endl
                << endl;
        }
    }
    buf << "}";
    return buf.str();
}

string BasicBlock::toString(core::Context ctx) {
    stringstream buf;
    buf << "block[id=" << this->id << "](";
    bool first = true;
    for (core::LocalVariable arg : this->args) {
        if (!first) {
            buf << ", ";
        }
        first = false;
        buf << arg.name.name(ctx).toString(ctx);
    }
    buf << ")" << endl;
    if (this->outerLoops > 0) {
        buf << "outerLoops: " << this->outerLoops << endl;
    }
    for (Binding &exp : this->exprs) {
        buf << exp.bind.name.name(ctx).toString(ctx) << " = " << exp.value->toString(ctx);
        if (exp.tpe) {
            buf << " : " << Strings::escapeCString(exp.tpe->toString(ctx));
        }
        buf << endl;
    }
    if (this->bexit.cond.exists()) {
        buf << this->bexit.cond.name.name(ctx).toString(ctx);
    } else {
        buf << "<unconditional>";
    }
    return buf.str();
}

Binding::Binding(core::LocalVariable bind, core::Loc loc, unique_ptr<Instruction> value)
    : bind(bind), loc(loc), value(move(value)) {}

Return::Return(core::LocalVariable what) : what(what) {
    categoryCounterInc("CFG", "Return");
}

string Return::toString(core::Context ctx) {
    return "return " + this->what.name.name(ctx).toString(ctx);
}

Send::Send(core::LocalVariable recv, core::NameRef fun, vector<core::LocalVariable> &args)
    : recv(recv), fun(fun), args(move(args)) {
    categoryCounterInc("CFG", "Send");
    histogramInc("CFG::Send::args", this->args.size());
}

FloatLit::FloatLit(double value) : value(value) {
    categoryCounterInc("CFG", "FloatLit");
}

string FloatLit::toString(core::Context ctx) {
    return to_string(this->value);
}

IntLit::IntLit(int64_t value) : value(value) {
    categoryCounterInc("CFG", "IntLit");
}

string IntLit::toString(core::Context ctx) {
    return to_string(this->value);
}

Ident::Ident(core::LocalVariable what) : what(what) {
    categoryCounterInc("CFG", "Ident");
}

Alias::Alias(core::SymbolRef what) : what(what) {
    categoryCounterInc("CFG", "Alias");
}

string Ident::toString(core::Context ctx) {
    return this->what.name.name(ctx).toString(ctx);
}

string Alias::toString(core::Context ctx) {
    return "alias " + this->what.info(ctx).name.name(ctx).toString(ctx);
}

string Send::toString(core::Context ctx) {
    stringstream buf;
    buf << this->recv.name.name(ctx).toString(ctx) << "." << this->fun.name(ctx).toString(ctx) << "(";
    bool isFirst = true;
    for (auto arg : this->args) {
        if (!isFirst) {
            buf << ", ";
        }
        isFirst = false;
        buf << arg.name.name(ctx).toString(ctx);
    }
    buf << ")";
    return buf.str();
}

string StringLit::toString(core::Context ctx) {
    return this->value.name(ctx).toString(ctx);
}

string SymbolLit::toString(core::Context ctx) {
    return "<symbol:" + this->value.name(ctx).toString(ctx) + ">";
}

string BoolLit::toString(core::Context ctx) {
    if (value) {
        return "true";
    } else {
        return "false";
    }
}

string Self::toString(core::Context ctx) {
    return "self";
}

string HashSplat::toString(core::Context ctx) {
    Error::notImplemented();
}

string ArraySplat::toString(core::Context ctx) {
    Error::notImplemented();
}

string LoadArg::toString(core::Context ctx) {
    stringstream buf;
    buf << "load_arg(";
    buf << this->receiver.name.name(ctx).toString(ctx);
    buf << "#";
    buf << this->method.name(ctx).toString(ctx);
    buf << ", " << this->arg << ")";
    return buf.str();
}

string Unanalyzable::toString(core::Context ctx) {
    return "<unanalyzable>";
}

string NotSupported::toString(core::Context ctx) {
    return "NotSupported(" + why + ")";
}

string Cast::toString(core::Context ctx) {
    stringstream buf;
    buf << "cast(";
    buf << this->value.name.toString(ctx);
    buf << ", ";
    buf << this->type->toString(ctx);
    buf << ");";
    return buf.str();
}

} // namespace cfg
} // namespace ruby_typer
