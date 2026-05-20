#pragma once
#ifndef SIGNAL_SOURCE_COMPONENT_H
#define SIGNAL_SOURCE_COMPONENT_H

#include <QElapsedTimer>
#include <QTableWidget>

#include "Components/AbstractComponent.h"
#include "Components/ComponentMacro.h"
#include "Components/ComponentSettingsDialog.h"

namespace rabbit_App::component {

COMPONENT_CLASS_DECLARATION(SignalSource)

/// @brief A single entry in the timing sequence.
/// Note: time_ms is stored in cycles (not ms) for all modes.
struct SequenceEntry {
  int time_ms = 0;  // Actually stores cycles, kept for backward compat
  bool value = false;

  bool operator==(const SequenceEntry &other) const {
    return time_ms == other.time_ms && value == other.value;
  }
};

/// @brief Signal generation mode.
enum class SignalMode {
  ConstantLow = 0,
  ConstantHigh = 1,
  SquareWave = 2,
  SinglePulse = 3,
  Sequence = 4,
};

/// @brief SignalSourceRawComponent class
/// Single-port programmable signal source.
class SignalSourceRawComponent : public AbstractRawComponent {
  Q_OBJECT

public:
  SignalSourceRawComponent(QWidget *parent = nullptr);
  virtual ~SignalSourceRawComponent();

  void setFrequency(int frequency) noexcept override {
    AbstractRawComponent::setFrequency(frequency);
    update();
  }

  void reset() override;
  void processReadData(QQueue<uint64_t> &read_queue) override;
  uint64_t getWriteData() const override;

  // Getters for settings
  SignalMode signalMode() const noexcept { return mode_; }
  int periodCycles() const noexcept { return period_ms_; }
  int dutyCyclePercent() const noexcept { return duty_cycle_percent_; }
  int delayCycles() const noexcept { return delay_ms_; }
  int pulseWidthCycles() const noexcept { return pulse_width_ms_; }
  const QVector<SequenceEntry> &sequence() const noexcept { return sequence_; }
  bool sequenceLoop() const noexcept { return sequence_loop_; }
  bool sequenceHold() const noexcept { return sequence_hold_; }

  // Setters for settings
  void setSignalMode(SignalMode mode) noexcept { mode_ = mode; }
  void setPeriodCycles(int period_cycles) noexcept { period_ms_ = period_cycles; }
  void setDutyCyclePercent(int duty) noexcept { duty_cycle_percent_ = duty; }
  void setDelayCycles(int delay_cycles) noexcept { delay_ms_ = delay_cycles; }
  void setPulseWidthCycles(int width_cycles) noexcept { pulse_width_ms_ = width_cycles; }
  void setSequence(const QVector<SequenceEntry> &seq) { sequence_ = seq; }
  void setSequenceLoop(bool loop) noexcept { sequence_loop_ = loop; }
  void setSequenceHold(bool hold) noexcept { sequence_hold_ = hold; }

  // Deprecated: kept for backward compatibility, do not use for new logic.
  // All time values are now stored directly in cycles.
  int timeToCycles(int time_ms) const {
    if (frequency_ <= 0) return time_ms;
    return time_ms * frequency_ / 1000;
  }
  int cyclesToTime(int cycles) const {
    if (frequency_ <= 0) return cycles;
    return cycles * 1000 / frequency_;
  }

  // Serialization helpers for project save/load
  QString sequenceToString() const;
  void sequenceFromString(const QString &str);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

  void initPorts() override;

private:
  bool computeOutput() const;
  QString modeString() const;
  void drawSequencePreview(QPainter &painter, const QRect &rect) const;

  SignalMode mode_ = SignalMode::SquareWave;
  int period_ms_ = 100;   // Actually stores cycles
  int duty_cycle_percent_ = 50;
  int delay_ms_ = 0;
  int pulse_width_ms_ = 100;

  QVector<SequenceEntry> sequence_;
  bool sequence_loop_ = false;
  bool sequence_hold_ = true;

  mutable int cycle_counter_ = 0;
};

/// @brief SignalSourceSettingsDialog class
/// Custom settings dialog for SignalSource with mode/parameters and active mode.
class SignalSourceSettingsDialog : public ComponentSettingsDialog {
  Q_OBJECT

public:
  SignalSourceSettingsDialog(SignalSourceRawComponent *raw_component,
                             AbstractComponent *component,
                             QWidget *parent = nullptr);
  virtual ~SignalSourceSettingsDialog();

  void accept() override;

private:
  void initSignalSettingsUi();
  void onModeChanged(int index);
  void onAddRow();
  void onDeleteRow();
  void refreshSequenceTable();

  SignalSourceRawComponent *raw_component_;

  QComboBox *mode_combo_;

  // Square wave / pulse parameters
  QLineEdit *period_edit_;
  QLineEdit *duty_cycle_edit_;
  QLineEdit *delay_edit_;
  QLineEdit *pulse_width_edit_;

  // Sequence parameters
  QTableWidget *sequence_table_;
  QCheckBox *loop_check_;
  QCheckBox *hold_check_;
  QPushButton *add_row_btn_;
  QPushButton *del_row_btn_;

  // Active mode
  QRadioButton *active_high_radio_;
  QRadioButton *active_low_radio_;
};

} // namespace rabbit_App::component

#endif // SIGNAL_SOURCE_COMPONENT_H
