// DemoBoardGeometry.h
#pragma once
#include "storage/KiCadLibrary.h"

namespace hvd {
/** Generate the synthetic demonstration board in millimetres.
 *
 * The PCB lies in XY, its centre is the origin, and +Z points toward the chip
 * and connector. Geometry has no manufacturer accuracy or electrical meaning.
 */
QVector<ModelVertex> demoBoardGeometry();
} // namespace hvd
