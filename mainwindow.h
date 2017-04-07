#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "Functions/Kernels/kernels.h"
#include "KDE/kerneldensityestimator.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void refreshKernelsTable();
    void refreshTargetFunctionTable();
        void uniformContributions();
            qreal countLastContribution();

    void on_pushButton_generate_clicked();
        void clearPlot();
        void addPlot(const QVector<qreal> *X, const QVector<qreal> *Y);
        void fillDomain(QVector<point *> *domain, point* prototypePoint);
        void generateSamples();
        QColor getRandomColor();
        void testKDE(kernelDensityEstimator* KDE, function* targetFunction);
            void fillTestDomain(QVector<point *> *domain, point* prototypePoint);

    void on_pushButton_clear_clicked();

    void on_spinBox_dimensionsNumber_editingFinished();



        void on_pushButton_countSmoothingParameters_clicked();

        void on_pushButton_addTargetFunction_clicked();

        void on_pushButton_removeTargetFunction_clicked();

private:
    Ui::MainWindow *ui;

    QVector<QVector<qreal>*> samples;
};

enum kernelSettingsColumns
{
    KERNEL_COLUMN_INDEX                 = 0,
    SMOOTHING_PARAMETER_COLUMN_INDEX    = 1,
    CARRIER_RESTRICTION_COLUMN_INDEX    = 2
};

enum targetFunctionSettingsColumns
{
    MEAN_COLUMN_INDEX           = 0,
    STDEV_COLUMN_INDEX          = 1,
    CONTRIBUTION_COLUMN_INDEX   = 2
};

enum smoothingParameterCountingMethods
{
    RANK_2_PLUG_IN    = 0,
    RANK_3_PLUG_IN    = 1
};

#endif // MAINWINDOW_H
