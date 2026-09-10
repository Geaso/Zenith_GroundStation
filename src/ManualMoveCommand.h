#pragma once

#include <QMap>
#include <QVariantMap>
#include <QtMath>
#include <cmath>

namespace ManualMoveCommand {

// One input contract for the UI and wire encoder. Unknown modes never fall back
// to a position command, and velocity inputs never become absolute positions.
inline QVariantMap build(const QString &mode, double x, double y, double z,
                         double yawDegrees, quint32 commandId)
{
    static const QMap<QString, int> modes = {
        {"XYZ_POS", 0}, {"XY_VEL_Z_POS", 1}, {"XYZ_VEL", 2},
        {"XYZ_POS_BODY", 3}, {"XYZ_VEL_BODY", 4},
        {"XY_VEL_Z_POS_BODY", 5}, {"LAT_LON_ALT", 8}
    };
    if (!modes.contains(mode) || !std::isfinite(x) || !std::isfinite(y)
        || !std::isfinite(z) || !std::isfinite(yawDegrees)) return {};
    if (mode == "LAT_LON_ALT" && (std::abs(x) > 90 || std::abs(y) > 180)) return {};

    const int moveMode = modes.value(mode);
    QVariantList position{0.0, 0.0, 0.0}, velocity{0.0, 0.0, 0.0};
    if (moveMode == 0 || moveMode == 3) position = {x, y, z};
    if (moveMode == 2 || moveMode == 4) velocity = {x, y, z};
    if (moveMode == 1 || moveMode == 5) {
        position[2] = z;
        velocity = {x, y, 0.0};
    }
    return {{"Agent_CMD", 4}, {"Control_Level", 0}, {"Move_mode", moveMode},
            {"position_ref", position}, {"velocity_ref", velocity},
            {"acceleration_ref", QVariantList{0.0, 0.0, 0.0}},
            {"yaw_ref", qDegreesToRadians(yawDegrees)}, {"Yaw_Rate_Mode", false},
            {"yaw_rate_ref", 0.0}, {"att_ref", QVariantList{0.0, 0.0, 0.0, 0.0}},
            {"latitude", moveMode == 8 ? x : 0.0},
            {"longitude", moveMode == 8 ? y : 0.0},
            {"altitude", moveMode == 8 ? z : 0.0},
            {"Command_ID", QVariant::fromValue(commandId)}};
}

} // namespace ManualMoveCommand
