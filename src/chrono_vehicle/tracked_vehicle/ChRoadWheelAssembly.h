// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2014 projectchrono.org
// All right reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Radu Serban
// =============================================================================
//
// Base class for a road wheel assembly (suspension).  A road wheel assembly
// contains a road wheel body (connected through a revolute joint to the chassis)
// with different suspension topologies.
//
// The reference frame for a vehicle follows the ISO standard: Z-axis up, X-axis
// pointing forward, and Y-axis towards the left of the vehicle.
//
// =============================================================================

#ifndef CH_ROAD_WHEEL_ASSEMBLY_H
#define CH_ROAD_WHEEL_ASSEMBLY_H

#include "chrono/core/ChShared.h"
#include "chrono/physics/ChBody.h"
#include "chrono/physics/ChBodyAuxRef.h"
#include "chrono/physics/ChLinkLock.h"

#include "chrono_vehicle/ChApiVehicle.h"
#include "chrono_vehicle/ChSubsysDefs.h"

#include "chrono_vehicle/tracked_vehicle/ChRoadWheel.h"

namespace chrono {
namespace vehicle {

///
///
///
class CH_VEHICLE_API ChRoadWheelAssembly : public ChShared {
  public:
    ChRoadWheelAssembly(const std::string& name  ///< [in] name of the subsystem
                        );

    virtual ~ChRoadWheelAssembly() {}

    /// Get the name identifier for this suspension subsystem.
    const std::string& GetName() const { return m_name; }

    /// Set the name identifier for this suspension subsystem.
    void SetName(const std::string& name) { m_name = name; }

    /// Return the type of track shoe consistent with this road wheel.
    TrackShoeType GetType() const { return m_type; }

    /// Get a handle to the road wheel body.
    ChSharedPtr<ChBody> GetWheel() const { return m_road_wheel->GetWheel(); }

    /// Get a handle to the revolute joint.
    ChSharedPtr<ChLinkLockRevolute> GetRevolute() const { return m_road_wheel->GetRevolute(); }

    /// Get the radius of the road wheel.
    double GetWheelRadius() const { return m_road_wheel->GetWheelRadius(); }

    /// Initialize this suspension subsystem.
    /// The suspension subsystem is initialized by attaching it to the specified
    /// chassis body at the specified location (with respect to and expressed in
    /// the reference frame of the chassis). It is assumed that the suspension
    /// reference frame is always aligned with the chassis reference frame.
    virtual void Initialize(ChSharedPtr<ChBodyAuxRef> chassis,  ///< [in] handle to the chassis body
                            const ChVector<>& location          ///< [in] location relative to the chassis frame
                            ) = 0;

  protected:
    std::string m_name;                     ///< name of the subsystem
    TrackShoeType m_type;                   ///< type of the track shoe matching this road wheel
    ChSharedPtr<ChRoadWheel> m_road_wheel;  ///< road-wheel subsystem
};

/// Vector of handles to road wheel assembly subsystems.
typedef std::vector<ChSharedPtr<ChRoadWheelAssembly> > ChRoadWheelAssemblyList;

}  // end namespace vehicle
}  // end namespace chrono

#endif