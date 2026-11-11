#include "QuickStartTracker.h"

#include "CircuitDocument.h"

#include "logicbench/Circuit.h"

#include <QCoreApplication>

namespace lbgui {

namespace {

const char* const kStepText[QuickStartTracker::kSteps] = {
    QT_TRANSLATE_NOOP("QuickStartTracker", "Drag two switches from the palette onto the canvas, and label them A and B."),
    QT_TRANSLATE_NOOP("QuickStartTracker", "Drag an XOR gate and an AND gate onto the canvas."),
    QT_TRANSLATE_NOOP("QuickStartTracker", "Drag from each switch to an input of both gates to wire them."),
    QT_TRANSLATE_NOOP("QuickStartTracker", "Drag two LEDs onto the canvas and label them S and C."),
    QT_TRANSLATE_NOOP("QuickStartTracker", "Wire the XOR output to S and the AND output to C."),
    QT_TRANSLATE_NOOP("QuickStartTracker", "Flip A and B, then press Ctrl+T and check the truth table."),
};

lb::ComponentId labelled(const lb::Circuit& c, const char* type, const char* label)
{
    for (lb::Component* part : c.componentsOfType(type))
        if (part->label() == label)
            return part->id();
    return lb::kNoComponent;
}

/// True if `from`'s output drives some input of a component of `type`.
bool drivesA(const lb::Circuit& c, lb::ComponentId from, const char* type)
{
    if (from == lb::kNoComponent)
        return false;
    for (const lb::Wire& w : c.wires())
        if (w.from.component == from && c.component(w.to.component)->typeName() == type)
            return true;
    return false;
}

/// True if a component of `type` drives `to`.
bool drivenByA(const lb::Circuit& c, lb::ComponentId to, const char* type)
{
    if (to == lb::kNoComponent)
        return false;
    const lb::Wire* w = c.driverOf(lb::PinRef{to, 0});
    return w && c.component(w->from.component)->typeName() == type;
}

} // namespace

QuickStartTracker::QuickStartTracker(CircuitDocument* document, QObject* parent)
    : QObject(parent)
    , m_document(document)
{
    connect(m_document, &CircuitDocument::structureChanged, this, &QuickStartTracker::evaluate);
    evaluate();
}

QString QuickStartTracker::stepText(int i)
{
    if (i < 0 || i >= kSteps)
        return QString();
    return QCoreApplication::translate("QuickStartTracker", kStepText[i]);
}

bool QuickStartTracker::isDone(int i) const
{
    return i >= 0 && i < kSteps && m_done[static_cast<std::size_t>(i)];
}

int QuickStartTracker::doneCount() const
{
    int n = 0;
    for (bool d : m_done)
        n += d ? 1 : 0;
    return n;
}

void QuickStartTracker::reset()
{
    m_done.fill(false);
    m_truthTableOpened = false;
    evaluate();
}

void QuickStartTracker::markTruthTableOpened()
{
    m_truthTableOpened = true;
    evaluate();
}

void QuickStartTracker::evaluate()
{
    const lb::Circuit& c = m_document->circuit();
    const lb::ComponentId a = labelled(c, "SWITCH", "A");
    const lb::ComponentId b = labelled(c, "SWITCH", "B");
    const lb::ComponentId s = labelled(c, "LED", "S");
    const lb::ComponentId carry = labelled(c, "LED", "C");
    const bool gates = !c.componentsOfType("XOR").empty() && !c.componentsOfType("AND").empty();

    const std::array<bool, kSteps> now = {
        a != lb::kNoComponent && b != lb::kNoComponent,
        gates,
        drivesA(c, a, "XOR") && drivesA(c, a, "AND") && drivesA(c, b, "XOR") && drivesA(c, b, "AND"),
        s != lb::kNoComponent && carry != lb::kNoComponent,
        drivenByA(c, s, "XOR") && drivenByA(c, carry, "AND"),
        m_truthTableOpened && drivenByA(c, s, "XOR") && drivenByA(c, carry, "AND"),
    };
    // A step stays ticked once done: undoing a wire later does not take the
    // tick away, which is what a tutorial's progress should do.
    for (int i = 0; i < kSteps; ++i) {
        if (now[static_cast<std::size_t>(i)] && !m_done[static_cast<std::size_t>(i)]) {
            m_done[static_cast<std::size_t>(i)] = true;
            emit progressed(i);
        }
    }
}

} // namespace lbgui
