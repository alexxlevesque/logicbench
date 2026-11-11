#include "logicbench/SopMinimizer.h"

#include <algorithm>
#include <bitset>
#include <cstdint>
#include <map>
#include <set>

namespace lb {

namespace {

int popcount(std::uint32_t x)
{
    return static_cast<int>(std::bitset<32>(x).count());
}

std::string defaultName(int i)
{
    return std::string(1, static_cast<char>('A' + i));
}

/// Cost of a cover: terms first, then literals.
struct Cost {
    std::size_t terms = 0;
    int literals = 0;
    bool operator<(const Cost& o) const { return terms != o.terms ? terms < o.terms : literals < o.literals; }
};

/// Minimum cover of the minterms by prime implicants. Minterm sets are
/// bitsets over minterm indices (at most 256 for 8 variables), so taking a
/// prime, testing coverage and comparing primes are a few word operations.
class CoverSearch {
public:
    using Rows = std::bitset<256>;

    CoverSearch(int variables, std::vector<Implicant> primes, const std::vector<std::uint32_t>& minterms)
        : m_n(variables), m_primes(std::move(primes))
    {
        for (const Implicant& p : m_primes) {
            Rows rows;
            for (std::size_t m = 0; m < minterms.size(); ++m)
                if (p.covers(minterms[m]))
                    rows.set(m);
            m_cover.push_back(rows);
            m_literals.push_back(p.literals(m_n));
        }
        for (std::size_t m = 0; m < minterms.size(); ++m)
            m_all.set(m);
        m_alive.assign(m_primes.size(), true);
    }

    std::vector<Implicant> run(bool& exact)
    {
        Rows required = m_all;
        std::vector<std::size_t> chosen;
        reduce(required, chosen);

        // Ways to cover each remaining minterm, by the surviving primes, best
        // candidates first (most rows still needed, then fewest literals).
        m_ways.assign(256, {});
        for (std::size_t m = 0; m < 256; ++m) {
            if (!required.test(m))
                continue;
            for (std::size_t p = 0; p < m_primes.size(); ++p)
                if (m_alive[p] && m_cover[p].test(m))
                    m_ways[m].push_back(p);
            std::sort(m_ways[m].begin(), m_ways[m].end(), [&](std::size_t a, std::size_t b) {
                const std::size_t ca = (m_cover[a] & required).count();
                const std::size_t cb = (m_cover[b] & required).count();
                return ca != cb ? ca > cb : m_literals[a] < m_literals[b];
            });
        }

        // A greedy cover first, so there is always an answer to keep.
        m_best = chosen;
        Rows left = required;
        while (left.any()) {
            std::size_t pick = 0;
            std::size_t gain = 0;
            for (std::size_t p = 0; p < m_primes.size(); ++p) {
                if (!m_alive[p])
                    continue;
                const std::size_t g = (m_cover[p] & left).count();
                if (g > gain || (g == gain && g > 0 && m_literals[p] < m_literals[pick])) {
                    gain = g;
                    pick = p;
                }
            }
            m_best.push_back(pick);
            left &= ~m_cover[pick];
        }
        m_bestCost = costOf(m_best);

        Cost start = costOf(chosen);
        search(required, chosen, start);
        exact = m_nodes <= kNodeBudget;

        std::vector<Implicant> terms;
        for (std::size_t p : m_best)
            terms.push_back(m_primes[p]);
        std::sort(terms.begin(), terms.end(), [](const Implicant& a, const Implicant& b) {
            return a.value != b.value ? a.value > b.value : a.mask < b.mask;
        });
        terms.erase(std::unique(terms.begin(), terms.end()), terms.end());
        return terms;
    }

private:
    static constexpr std::size_t kNodeBudget = 100000;

    Cost costOf(const std::vector<std::size_t>& set) const
    {
        Cost c;
        for (std::size_t p : set) {
            ++c.terms;
            c.literals += m_literals[p];
        }
        return c;
    }

    /// Essential primes and dominated primes, repeated until nothing changes.
    void reduce(Rows& required, std::vector<std::size_t>& chosen)
    {
        bool progress = true;
        while (progress && required.any()) {
            progress = false;

            // A prime that covers nothing still needed, or covers a subset of
            // what another no-more-expensive prime covers, is never needed.
            for (std::size_t p = 0; p < m_primes.size(); ++p) {
                if (!m_alive[p])
                    continue;
                const Rows mine = m_cover[p] & required;
                if (mine.none()) {
                    m_alive[p] = false;
                    progress = true;
                    continue;
                }
                for (std::size_t q = 0; q < m_primes.size(); ++q) {
                    if (q == p || !m_alive[q])
                        continue;
                    const Rows theirs = m_cover[q] & required;
                    if ((mine & ~theirs).any())
                        continue;
                    const bool cheaper = m_literals[q] < m_literals[p];
                    const bool tieBroken = m_literals[q] == m_literals[p] && (theirs != mine || q < p);
                    if (cheaper || tieBroken) {
                        m_alive[p] = false;
                        progress = true;
                        break;
                    }
                }
            }

            // A minterm only one surviving prime covers makes that prime essential.
            for (std::size_t m = 0; m < 256; ++m) {
                if (!required.test(m))
                    continue;
                std::size_t count = 0, only = 0;
                for (std::size_t p = 0; p < m_primes.size() && count < 2; ++p) {
                    if (m_alive[p] && m_cover[p].test(m)) {
                        ++count;
                        only = p;
                    }
                }
                if (count == 1) {
                    chosen.push_back(only);
                    required &= ~m_cover[only];
                    m_alive[only] = false;
                    progress = true;
                }
            }
        }
    }

    void search(const Rows& required, std::vector<std::size_t>& chosen, const Cost& here)
    {
        if (required.none()) {
            if (here < m_bestCost) {
                m_best = chosen;
                m_bestCost = here;
            }
            return;
        }
        if (++m_nodes > kNodeBudget)
            return;
        if (here.terms + 1 > m_bestCost.terms)
            return;                                    // at least one more term is needed

        // Branch on the minterm with the fewest ways to cover it.
        std::size_t pick = 256;
        std::size_t fewest = SIZE_MAX;
        for (std::size_t m = 0; m < 256; ++m) {
            if (required.test(m) && m_ways[m].size() < fewest) {
                fewest = m_ways[m].size();
                pick = m;
            }
        }
        for (std::size_t p : m_ways[pick]) {
            const Rows rest = required & ~m_cover[p];
            Cost next = here;
            ++next.terms;
            next.literals += m_literals[p];
            // Equal terms and more rows still to cover can never win.
            if (next.terms > m_bestCost.terms || (next.terms == m_bestCost.terms && rest.any()))
                continue;
            if (rest.none() && !(next < m_bestCost))
                continue;
            chosen.push_back(p);
            search(rest, chosen, next);
            chosen.pop_back();
            if (m_nodes > kNodeBudget)
                return;
        }
    }

    int m_n;
    std::vector<Implicant> m_primes;
    std::vector<Rows> m_cover;
    std::vector<int> m_literals;
    std::vector<bool> m_alive;
    std::vector<std::vector<std::size_t>> m_ways;
    Rows m_all;
    std::vector<std::size_t> m_best;
    Cost m_bestCost;
    std::size_t m_nodes = 0;
};

} // namespace

int Implicant::literals(int variables) const noexcept
{
    return variables - popcount(mask & ((1u << variables) - 1u));
}

bool SopResult::isConstantOne() const noexcept
{
    return terms.size() == 1 && terms.front().literals(variables) == 0;
}

int SopResult::literalCount() const noexcept
{
    int total = 0;
    for (const Implicant& t : terms)
        total += t.literals(variables);
    return total;
}

bool SopResult::evaluate(std::uint32_t row) const noexcept
{
    return std::any_of(terms.begin(), terms.end(), [row](const Implicant& t) { return t.covers(row); });
}

namespace {

std::string termText(const SopResult& r, const Implicant& t, const char* andSep, bool prime)
{
    std::string text;
    for (int v = 0; v < r.variables; ++v) {
        const std::uint32_t bit = 1u << (r.variables - 1 - v);
        if (t.mask & bit)
            continue;
        if (!text.empty())
            text += andSep;
        const std::string& name = r.names[static_cast<std::size_t>(v)];
        if (prime)
            text += (t.value & bit) ? name : name + "′";
        else
            text += (t.value & bit) ? name : "!" + name;
    }
    return text;
}

} // namespace

std::string SopResult::expression() const
{
    const std::string first = names.empty() ? std::string("A") : names.front();
    if (isConstantZero())
        return first + " & !" + first;
    if (isConstantOne())
        return first + " | !" + first;
    std::string text;
    for (const Implicant& t : terms) {
        if (!text.empty())
            text += " | ";
        const std::string term = termText(*this, t, " & ", false);
        text += (t.literals(variables) > 1 && terms.size() > 1) ? "(" + term + ")" : term;
    }
    return text;
}

std::string SopResult::display() const
{
    if (isConstantZero())
        return "0";
    if (isConstantOne())
        return "1";
    std::string text;
    for (const Implicant& t : terms) {
        if (!text.empty())
            text += " + ";
        text += termText(*this, t, "·", true);
    }
    return text;
}

std::vector<Implicant> SopMinimizer::primeImplicants(int variables,
                                                     const std::vector<std::uint32_t>& minterms,
                                                     const std::vector<std::uint32_t>& dontCares)
{
    std::set<Implicant> current;
    for (std::uint32_t m : minterms)
        current.insert({m, 0});
    for (std::uint32_t m : dontCares)
        current.insert({m, 0});

    std::set<Implicant> primes;
    while (!current.empty()) {
        std::set<Implicant> next;
        std::set<Implicant> combined;
        // Two terms combine when they eliminate the same variables and differ
        // in exactly one of the rest.
        std::map<std::uint32_t, std::vector<Implicant>> byMask;
        for (const Implicant& i : current)
            byMask[i.mask].push_back(i);
        for (auto& entry : byMask) {
            std::vector<Implicant>& group = entry.second;
            for (std::size_t a = 0; a < group.size(); ++a) {
                for (std::size_t b = a + 1; b < group.size(); ++b) {
                    const std::uint32_t diff = group[a].value ^ group[b].value;
                    if (popcount(diff) != 1)
                        continue;
                    next.insert({group[a].value & ~diff, group[a].mask | diff});
                    combined.insert(group[a]);
                    combined.insert(group[b]);
                }
            }
        }
        for (const Implicant& i : current)
            if (combined.count(i) == 0)
                primes.insert(i);
        current = next;
    }
    (void)variables;
    return std::vector<Implicant>(primes.begin(), primes.end());
}

SopResult SopMinimizer::minimize(int variables, const std::vector<std::uint32_t>& minterms,
                                 const std::vector<std::uint32_t>& dontCares,
                                 const std::vector<std::string>& names)
{
    SopResult result;
    result.variables = variables;
    for (int i = 0; i < variables; ++i)
        result.names.push_back(i < static_cast<int>(names.size()) ? names[static_cast<std::size_t>(i)]
                                                                   : defaultName(i));
    std::set<std::uint32_t> ones(minterms.begin(), minterms.end());
    for (std::uint32_t d : dontCares)
        ones.erase(d);                                 // a row cannot be both
    result.minterms.assign(ones.begin(), ones.end());
    if (result.minterms.empty())
        return result;                                 // constant 0

    std::vector<Implicant> primes = primeImplicants(variables, result.minterms, dontCares);
    CoverSearch search(variables, std::move(primes), result.minterms);
    result.terms = search.run(result.exact);
    return result;
}

} // namespace lb
