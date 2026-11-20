#include "ui/components/dialog_window_policy.hpp"
#include <QApplication>
#include <QFontDatabase>
#include <QInputDialog>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <cstdio>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    const QString mode = QString::fromLatin1(argv[1]);
    const bool atRight = QString::fromLatin1(argv[2]) == "right";
    QWidget host;
    host.resize(260, 150);
    QPushButton anchor("Choose", &host);
    anchor.setGeometry(atRight ? 170 : 10, 108, 80, 30);
    const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
    host.move(atRight ? available.right() + 1 - host.width() : available.left() + 10,
              available.bottom() + 1 - host.height());
    host.show();
    QCoreApplication::processEvents();
    QInputDialog dialog(&host);
    dialog.setWindowTitle("Choose collide");
    dialog.setLabelText("Value");
    dialog.setComboBoxItems({"None (0)", "Sphere (1)", "Sphere tree (2)", "Face (3)"});
    dialog.setComboBoxEditable(false);
    dialog.setTextValue("Sphere tree (2)");
    dialog.adjustSize();
    sed::ui::applyDialogWindowPolicy(dialog);
    auto* box = qobject_cast<QVBoxLayout*>(dialog.layout());
    if (!box) return 2;
    const QMargins margins = box->contentsMargins();
    box->addStrut(std::max(0, dialog.minimumWidth() - margins.left() - margins.right()));
    box->activate();

    int lateObservations = 0;
    bool lateControls = true;
    bool lateAdjacent = true;
    bool earlyAdjacent = false;
    auto position = [&] { sed::ui::positionDialogNearWidget(dialog, &anchor); };
    auto observe = [&](const char* phase, bool required) {
        const QRect ar(anchor.mapToGlobal(QPoint{}), anchor.size());
        const QRect dr = dialog.frameGeometry();
        QStyleOptionTitleBar option;
        option.initFrom(&dialog);
        option.fontMetrics = QFontMetrics(QFontDatabase::systemFont(QFontDatabase::TitleFont), &dialog);
        option.rect = QRect(0, 0, dr.width(), std::max(option.fontMetrics.height() + 8,
            dialog.style()->pixelMetric(QStyle::PM_TitleBarHeight, nullptr, &dialog)));
        option.text = dialog.windowTitle();
        option.titleBarFlags = dialog.windowFlags();
        option.titleBarState = static_cast<int>(dialog.windowState());
        const auto label = dialog.style()->subControlRect(QStyle::CC_TitleBar, &option,
                                                         QStyle::SC_TitleBarLabel, &dialog);
        const bool inkFits = label.isValid() && label.width() >= option.fontMetrics.horizontalAdvance(option.text);
        const bool active = dialog.isModal() && QApplication::activeModalWidget() == &dialog;
        const bool right = dr.left() >= ar.right() && dr.left() - ar.right() <= 14;
        const bool left = ar.left() >= dr.right() && ar.left() - dr.right() <= 14;
        const bool adjacent = atRight ? left : right;
        const int floor = sed::ui::minimumDialogWidthForCaption(dialog);
        const bool controls = active && available.contains(dr) && inkFits && dialog.minimumWidth() >= floor
            && !dialog.windowFlags().testFlag(Qt::WindowMinimizeButtonHint)
            && !dialog.windowFlags().testFlag(Qt::WindowMaximizeButtonHint)
            && dialog.textValue() == "Sphere tree (2)";
        if (required) {
            ++lateObservations;
            lateControls &= controls;
            lateAdjacent &= adjacent;
        }
        std::printf("%s anchor=%d,%d,%dx%d frame=%d,%d,%dx%d client=%dx%d min=%d policy=%d ink=%d activeModal=%d adjacent=%d controls=%d\n",
            phase, ar.x(), ar.y(), ar.width(), ar.height(), dr.x(), dr.y(), dr.width(), dr.height(),
            dialog.width(), dialog.height(), dialog.minimumWidth(), floor, inkFits, active, adjacent, controls);
        return adjacent;
    };
    // Register the initial observation before the caller selects an opening strategy,
    // as in the editor's integration test. Later checks verify settled native placement.
    QTimer::singleShot(0, &dialog, [&] { earlyAdjacent = observe("initial", false); });
    QTimer::singleShot(60, &dialog, [&] {
        box->invalidate();
        box->activate();
        observe("reactivated", true);
    });
    QTimer::singleShot(200, &dialog, [&] { observe("settled", true); dialog.accept(); });

    if (mode != "hidden" && mode != "queued") dialog.setWindowModality(Qt::ApplicationModal);
    if (mode == "shown" || mode == "shown-synced") dialog.show();
    if (mode == "shown-synced") QGuiApplication::sync();
    position();
    if (mode == "queued" || mode == "modal-queued")
        QTimer::singleShot(0, &dialog, position);
    QTimer::singleShot(4000, &dialog, &QDialog::reject);
    const int result = dialog.exec();
    lateControls &= lateObservations == 2;
    std::printf("RESULT %s %s controls=%d early=%d settled=%d accepted=%d\n",
        argv[1], argv[2], lateControls, earlyAdjacent, lateAdjacent, result == QDialog::Accepted);
    return lateControls && lateAdjacent && result == QDialog::Accepted ? 0 : 1;
}
