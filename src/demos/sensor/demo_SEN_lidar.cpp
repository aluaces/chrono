// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2019 projectchrono.org
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Asher Elmquist
// =============================================================================
//
// Chrono demonstration of a lidar sensor
// Simple demonstration of certain filters and the visualization of a static mesh
//
// =============================================================================

#include <cmath>
#include <cstdio>
#include <iomanip>

#include "chrono/assets/ChTriangleMeshShape.h"
#include "chrono/assets/ChVisualMaterial.h"
#include "chrono/assets/ChVisualization.h"
#include "chrono/geometry/ChTriangleMeshConnected.h"
#include "chrono/physics/ChBodyEasy.h"
#include "chrono/physics/ChSystemNSC.h"
#include "chrono/utils/ChUtilsCreators.h"
#include "chrono_thirdparty/filesystem/path.h"

#include "chrono_sensor/ChLidarSensor.h"
#include "chrono_sensor/ChSensorManager.h"
#include "chrono_sensor/filters/ChFilterAccess.h"
#include "chrono_sensor/filters/ChFilterPCfromDepth.h"
#include "chrono_sensor/filters/ChFilterVisualize.h"
#include "chrono_sensor/filters/ChFilterVisualizePointCloud.h"
#include "chrono_sensor/filters/ChFilterLidarReduce.h"
#include "chrono_sensor/filters/ChFilterLidarNoise.h"
#include "chrono_sensor/filters/ChFilterSavePtCloud.h"

using namespace chrono;
using namespace chrono::geometry;
using namespace chrono::sensor;

// -----------------------------------------------------------------------------
// Lidar parameters
// -----------------------------------------------------------------------------

// Noise model attached to the sensor
enum NoiseModel {
    CONST_NORMAL_XYZI,  // Gaussian noise with constant mean and standard deviation
    NONE                // No noise model
};
NoiseModel noise_model = NONE;

// Lidar method for generating data
// Just RAYCAST for now
// TODO: implement PATH_TRACE
LidarModelType lidar_model = RAYCAST;

// Lidar return mode
// Either STRONGEST_RETURN, MEAN_RETURN, FIRST_RETURN, LAST_RETURN
LidarReturnMode return_mode = STRONGEST_RETURN;

// Update rate in Hz
int update_rate = 5;

// Number of horizontal and vertical samples
unsigned int horizontal_samples = 4500;
unsigned int vertical_samples = 32;

// Horizontal and vertical field of view (radians)
float horizontal_fov = 2 * CH_C_PI;   // 360 degree scan
float max_vert_angle = CH_C_PI / 12;  // 15 degrees up
float min_vert_angle = -CH_C_PI / 6;  // 30 degrees down

// Lag time
float lag = 0;

// Collection window for the lidar
float collection_time = 1 / double(update_rate);  // typically 1/update rate

// -----------------------------------------------------------------------------
// Simulation parameters
// -----------------------------------------------------------------------------

// Simulation step size
double step_size = 1e-3;

// Simulation end time
float end_time = 20.0f;

// Save lidar point clouds
bool save = false;

// Render lidar point clouds
bool vis = true;

// Output directories
const std::string out_dir = "SENSOR_OUTPUT/LIDAR_DEMO/";

int main(int argc, char* argv[]) {
    GetLog() << "Copyright (c) 2019 projectchrono.org\nChrono version: " << CHRONO_VERSION << "\n\n";

    // -----------------
    // Create the system
    // -----------------
    ChSystemNSC mphysicalSystem;

    // ----------------------------------
    // add a mesh to be sensed by a lidar
    // ----------------------------------
    auto mmesh = chrono_types::make_shared<ChTriangleMeshConnected>();
    mmesh->LoadWavefrontMesh(GetChronoDataFile("vehicle/hmmwv/hmmwv_chassis.obj"), false, true);
    mmesh->Transform(ChVector<>(0, 0, 0), ChMatrix33<>(1));  // scale to a different size

    auto trimesh_shape = chrono_types::make_shared<ChTriangleMeshShape>();
    trimesh_shape->SetMesh(mmesh);
    trimesh_shape->SetName("HMMWV Chassis Mesh");
    trimesh_shape->SetStatic(true);

    auto mesh_body = chrono_types::make_shared<ChBody>();
    mesh_body->SetPos({0, 0, 0});
    mesh_body->AddAsset(trimesh_shape);
    mesh_body->SetBodyFixed(true);
    mphysicalSystem.Add(mesh_body);

    // --------------------------------------------
    // add a few box bodies to be sensed by a lidar
    // --------------------------------------------
    auto box_body = chrono_types::make_shared<ChBodyEasyBox>(100, 100, 1, 1000, true, false);
    box_body->SetPos({0, 0, -3});
    box_body->SetBodyFixed(true);
    mphysicalSystem.Add(box_body);

    auto box_body_1 = chrono_types::make_shared<ChBodyEasyBox>(100, 1, 100, 1000, true, false);
    box_body_1->SetPos({0, -10, -3});
    box_body_1->SetBodyFixed(true);
    mphysicalSystem.Add(box_body_1);

    auto box_body_2 = chrono_types::make_shared<ChBodyEasyBox>(100, 1, 100, 1000, true, false);
    box_body_2->SetPos({0, 10, -3});
    box_body_2->SetBodyFixed(true);
    mphysicalSystem.Add(box_body_2);

    // -----------------------
    // Create a sensor manager
    // -----------------------
    auto manager = chrono_types::make_shared<ChSensorManager>(&mphysicalSystem);
    // int num_keyframes = 1 / (update_rate * step_size);
    // manager->SetKeyframeSize(num_keyframes);

    // -----------------------------------------------
    // Create a lidar and add it to the sensor manager
    // -----------------------------------------------
    auto offset_pose = chrono::ChFrame<double>({-4, 0, 4}, Q_from_AngAxis(0, {0, 1, 0}));
    auto lidar = chrono_types::make_shared<ChLidarSensor>(box_body,            // body lidar is attached to
                                                          update_rate,         // scanning rate in Hz
                                                          offset_pose,         // offset pose
                                                          horizontal_samples,  // number of horizontal samples
                                                          vertical_samples,    // number of vertical channels
                                                          horizontal_fov,      // horizontal field of view
                                                          max_vert_angle, min_vert_angle  // vertical field of view
    );
    lidar->SetName("Lidar Sensor 1");
    lidar->SetLag(lag);
    lidar->SetCollectionWindow(collection_time);

    // -----------------------------------------------------------------
    // Create a filter graph for post-processing the data from the lidar
    // -----------------------------------------------------------------
    // Add a noise model filter to the camera sensor
    switch (noise_model) {
        case CONST_NORMAL_XYZI:
            lidar->PushFilter(chrono_types::make_shared<ChFilterLidarNoiseXYZI>(0.01f, 0.001f, 0.001f, 0.01f));
            break;
        case NONE:
            // Don't add any noise models
            break;
    }

    // Provides the host access to the Depth,Intensity data
    lidar->PushFilter(chrono_types::make_shared<ChFilterDIAccess>());

    // Renders the raw lidar data
    if (vis)
        lidar->PushFilter(chrono_types::make_shared<ChFilterVisualize>(horizontal_samples / 2, vertical_samples * 5,
                                                                       "Raw Lidar Depth Data"));

    // Convert Depth,Intensity data to XYZI point cloud data
    lidar->PushFilter(chrono_types::make_shared<ChFilterPCfromDepth>());

    // Render the point cloud
    if (vis)
        lidar->PushFilter(chrono_types::make_shared<ChFilterVisualizePointCloud>(640, 480, "Lidar Point Cloud"));

    // Access the lidar data as an XYZI buffer
    lidar->PushFilter(chrono_types::make_shared<ChFilterXYZIAccess>());

    // Save the XYZI data
    if (save)
        lidar->PushFilter(chrono_types::make_shared<ChFilterSavePtCloud>(out_dir + "ideal/"));

    // add sensor to the manager
    manager->AddSensor(lidar);

    // -----------------------------------------------------------------------
    // Create a multi-sample lidar, where each beam is traced by multiple rays
    // -----------------------------------------------------------------------
    unsigned int sample_radius = 5;  // radius of samples to use, 1->1 sample,2->9 samples, 3->25 samples...
    float divergence_angle = 0.003;  // 3mm radius (as cited by velodyne)
    auto lidar2 = chrono_types::make_shared<ChLidarSensor>(box_body,            // body lidar is attached to
                                                           update_rate,         // scanning rate in Hz
                                                           offset_pose,         // offset pose
                                                           horizontal_samples,  // number of horizontal samples
                                                           vertical_samples,    // number of vertical channels
                                                           horizontal_fov,      // horizontal field of view
                                                           max_vert_angle, min_vert_angle,  // vertical field of view
                                                           sample_radius,                   // sample radius
                                                           divergence_angle,                // divergence angle
                                                           return_mode,                     // return mode for the lidar
                                                           lidar_model  // method/model to use for generating data
    );
    lidar2->SetName("Lidar Sensor 2");
    lidar2->SetLag(lag);
    lidar2->SetCollectionWindow(collection_time);

    // -----------------------------------------------------------------
    // Create a filter graph for post-processing the data from the lidar
    // -----------------------------------------------------------------
    // Add a noise model filter to the camera sensor
    switch (noise_model) {
        case CONST_NORMAL_XYZI:
            lidar2->PushFilter(chrono_types::make_shared<ChFilterLidarNoiseXYZI>(0.01f, 0.001f, 0.001f, 0.01f));
            break;
        case NONE:
            // Don't add any noise models
            break;
    }

    // Provides the host access to the Depth,Intensity data
    lidar2->PushFilter(chrono_types::make_shared<ChFilterDIAccess>());

    // Renders the raw lidar data
    if (vis)
        lidar2->PushFilter(
            chrono_types::make_shared<ChFilterVisualize>(horizontal_samples, vertical_samples, "Raw Lidar Depth Data"));

    // Convert Depth,Intensity data to XYZI point cloud data
    lidar2->PushFilter(chrono_types::make_shared<ChFilterPCfromDepth>());

    // Render the point cloud
    if (vis)
        lidar2->PushFilter(chrono_types::make_shared<ChFilterVisualizePointCloud>(640, 480, "Lidar Point Cloud"));

    // Access the lidar data as an XYZI buffer
    lidar2->PushFilter(chrono_types::make_shared<ChFilterXYZIAccess>());

    // Save the XYZI data
    if (save)
        lidar2->PushFilter(chrono_types::make_shared<ChFilterSavePtCloud>(out_dir + "model/"));

    // add sensor to the manager
    manager->AddSensor(lidar2);

    // ---------------
    // Simulate system
    // ---------------
    double render_time = 0;
    float orbit_radius = 10.f;
    float orbit_rate = 2.5;
    float ch_time = 0.0;
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

    UserDIBufferPtr di_ideal_ptr;
    UserXYZIBufferPtr xyzi_ideal_ptr;
    UserXYZIBufferPtr xyzi_model_ptr;
    while (ch_time < end_time) {
        mesh_body->SetRot(Q_from_AngAxis(ch_time * orbit_rate, {0, 0, 1}));
        // Access the DI buffer from the ideal lidar
        di_ideal_ptr = lidar->GetMostRecentBuffer<UserDIBufferPtr>();
        if (di_ideal_ptr->Buffer) {
            std::cout << "DI buffer recieved from ideal lidar model." << std::endl;
            std::cout << "\tLidar resolution: " << di_ideal_ptr->Width << "x" << di_ideal_ptr->Height << std::endl;
            std::cout << "\tFirst Point: [" << di_ideal_ptr->Buffer[0].range << ", "
                      << di_ideal_ptr->Buffer[0].intensity << "]" << std::endl
                      << std::endl;
        }

        // Access the XYZI buffer from the ideal lidar
        xyzi_ideal_ptr = lidar->GetMostRecentBuffer<UserXYZIBufferPtr>();
        if (xyzi_ideal_ptr->Buffer) {
            std::cout << "XYZI buffer recieved from ideal lidar model." << std::endl;
            std::cout << "\tFirst Point: [";
            std::cout << xyzi_ideal_ptr->Buffer[0].x << ", ";
            std::cout << xyzi_ideal_ptr->Buffer[0].y << ", ";
            std::cout << xyzi_ideal_ptr->Buffer[0].z << ", ";
            std::cout << xyzi_ideal_ptr->Buffer[0].intensity;
            std::cout << "]" << std::endl << std::endl;
        }

        // Access the XYZI buffer from the model lidar
        xyzi_model_ptr = lidar2->GetMostRecentBuffer<UserXYZIBufferPtr>();
        if (xyzi_model_ptr->Buffer && xyzi_ideal_ptr->Buffer) {
            // Calculate the mean error between the ideal and model lidar
            double total_error = 0;
            int samples = 0;
            for (int i = 0; i < xyzi_ideal_ptr->Height; i++) {
                for (int j = 0; j < xyzi_ideal_ptr->Width; j++) {
                    if (xyzi_ideal_ptr->Buffer[i * xyzi_ideal_ptr->Width + j].intensity > 1e-3 &&
                        xyzi_model_ptr->Buffer[i * xyzi_ideal_ptr->Width + j].intensity > 1e-3) {
                        total_error += abs(xyzi_ideal_ptr->Buffer[i * xyzi_ideal_ptr->Width + j].y -
                                           xyzi_model_ptr->Buffer[i * xyzi_ideal_ptr->Width + j].y);
                        total_error += abs(xyzi_ideal_ptr->Buffer[i * xyzi_ideal_ptr->Width + j].z -
                                           xyzi_model_ptr->Buffer[i * xyzi_ideal_ptr->Width + j].z);
                        total_error += abs(xyzi_ideal_ptr->Buffer[i * xyzi_ideal_ptr->Width + j].intensity -
                                           xyzi_model_ptr->Buffer[i * xyzi_ideal_ptr->Width + j].intensity);
                        samples++;
                    }
                }
            }
            std::cout << "Mean difference in lidar values: " << total_error / samples << std::endl << std::endl;
        }

        // Update sensor manager
        // Will render/save/filter automatically
        manager->Update();

        // Perform step of dynamics
        mphysicalSystem.DoStepDynamics(step_size);

        // Get the current time of the simulation
        ch_time = (float)mphysicalSystem.GetChTime();
    }
    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> wall_time = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
    std::cout << "Simulation time: " << ch_time << "s, wall time: " << wall_time.count() << "s.\n";

    return 0;
}