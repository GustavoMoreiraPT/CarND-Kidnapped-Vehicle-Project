/**
 * particle_filter.cpp
 *
 * Created on: Dec 12, 2016
 * Author: Tiffany Huang
 */

#include "particle_filter.h"

#include <math.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "helper_functions.h"

using namespace std;

static default_random_engine gen;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
  /**
   * TODO: Set the number of particles. Initialize all particles to 
   *   first position (based on estimates of x, y, theta and their uncertainties
   *   from GPS) and all weights to 1. 
   * TODO: Add random Gaussian noise to each particle.
   * NOTE: Consult particle_filter.h for more information about this method 
   *   (and others in this file).
   */
    num_particles = 100;  // TODO: Set the number of particles

    normal_distribution<double> x_init(0, std[0]);
    normal_distribution<double> y_init(0, std[1]);
    normal_distribution<double> theta_init(0, std[2]);

    for (int i = 0; i < num_particles; i++) {
        Particle particle;
        particle.id = i;
        particle.x = x + x_init(gen);
        particle.y = y + y_init(gen);
        particle.theta = theta + theta_init(gen);
        particle.weight = 1.0;

        particles.push_back(particle);
    }

    is_initialized = true;

}

void ParticleFilter::prediction(double delta_t, double std_pos[], 
                                double velocity, double yaw_rate) {
  /**
   * TODO: Add measurements to each particle and add random Gaussian noise.
   * NOTE: When adding noise you may find std::normal_distribution 
   *   and std::default_random_engine useful.
   *  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
   *  http://www.cplusplus.com/reference/random/default_random_engine/
   */
    for (int i = 0; i < num_particles; i++) {
        double predicted_x;
        double predicted_y;
        double predicted_theta = particles[i].theta;

        if (fabs(yaw_rate) == 0) {
            predicted_x = particles[i].x + velocity * cos(particles[i].theta) * delta_t;
            predicted_y = particles[i].y + velocity * sin(particles[i].theta) * delta_t;
        }
        else {
            predicted_x = particles[i].x + (velocity / yaw_rate) * (sin(particles[i].theta + (yaw_rate * delta_t)) - sin(particles[i].theta));
            predicted_y = particles[i].y + (velocity / yaw_rate) * (cos(particles[i].theta) - cos(particles[i].theta + (yaw_rate * delta_t)));
            predicted_theta = predicted_theta + (yaw_rate * delta_t);
        }

        normal_distribution<double> new_x_dist(predicted_x, std_pos[0]);
        normal_distribution<double> new_y_dist(predicted_y, std_pos[1]);
        normal_distribution<double> new_theta_dist(predicted_theta, std_pos[2]);

        particles[i].x = new_x_dist(gen);
        particles[i].y = new_y_dist(gen);
        particles[i].theta = new_theta_dist(gen);
    }
}

void ParticleFilter::dataAssociation(vector<LandmarkObs> predicted, 
                                     vector<LandmarkObs>& observations) {
  /**
   * TODO: Find the predicted measurement that is closest to each 
   *   observed measurement and assign the observed measurement to this 
   *   particular landmark.
   * NOTE: this method will NOT be called by the grading code. But you will 
   *   probably find it useful to implement this method and use it as a helper 
   *   during the updateWeights phase.
   */

   int i = 0;
    for (auto o : observations) {
        double minimumDistance = numeric_limits<double>::max();

        int best_neighbor_id = -1;

        for (auto p : predicted) {

            double currentDistance = dist(o.x, o.y, p.x, p.y); //helper_functions.h 

            if (currentDistance < minimumDistance) {
                minimumDistance = currentDistance;
                best_neighbor_id = p.id;
            }
        }

        observations[i].id = best_neighbor_id;

        i++;
    }
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
                                   const vector<LandmarkObs> &observations, 
                                   const Map &map_landmarks) {
  /**
   * TODO: Update the weights of each particle using a mult-variate Gaussian 
   *   distribution. You can read more about this distribution here: 
   *   https://en.wikipedia.org/wiki/Multivariate_normal_distribution
   * NOTE: The observations are given in the VEHICLE'S coordinate system. 
   *   Your particles are located according to the MAP'S coordinate system. 
   *   You will need to transform between the two systems. Keep in mind that
   *   this transformation requires both rotation AND translation (but no scaling).
   *   The following is a good resource for the theory:
   *   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
   *   and the following is a good resource for the actual equation to implement
   *   (look at equation 3.33) http://planning.cs.uiuc.edu/node99.html
   */

   //First part: iterate over the particle and 
   for(int i = 0; i < num_particles; i++){

       vector<LandmarkObs> predicted_landmarks;

        //only keep landmarks within the maximum range of the sensor_range
       for(auto landmark: map_landmarks.landmark_list){
           if(dist(particles[i].x, particles[i].y, landmark.x_f, landmark.y_f) <= sensor_range){
               predicted_landmarks.push_back(LandmarkObs{ landmark.id_i, landmark.x_f, landmark.y_f });
		   }
       }

       //transform observations from vehicle map coordinates to map coordinates
       vector<LandmarkObs> transformed_observations;
       for(auto observation: observations){
        double transformed_x = cos(particles[i].theta) * observation.x - sin(particles[i].theta) * observation.y + particles[i].x;
        double transformed_y = sin(particles[i].theta) * observation.x + cos(particles[i].theta) * observation.y + particles[i].y;
        transformed_observations.push_back(LandmarkObs{observation.id, transformed_x, transformed_y});
	   }

       //include dataAssociation step on the current particle[i]
       dataAssociation(predicted_landmarks, transformed_observations);

       //for each iteration, reassign the weight to 1 because of previous resampling
       particles[i].weight = 1.0;
       for (auto transformed_observation : transformed_observations) {
           double observed_x = transformed_observation.x;
           double observed_y = transformed_observation.y;
           int calculated_associated_prediction = transformed_observation.id;
           double predicted_x;
           double predicted_y;

           //compare the predicted coordinates with the current observation
           for (auto landmark : predicted_landmarks) {
               if (landmark.id == calculated_associated_prediction) {
                   predicted_x = landmark.x;
                   predicted_y = landmark.y;
               }
           }

           //apply multivariate gaussian formula to calculate the new weight
           double new_weight = (1 / (2 * M_PI * std_landmark[0] * std_landmark[1])) * exp(-(pow(predicted_x - observed_x, 2) / (2 * pow(std_landmark[0], 2)) + (pow(predicted_y - observed_y, 2) / (2 * pow(std_landmark[1], 2)))));

           particles[i].weight *= new_weight;
       }
   }
}

void ParticleFilter::resample() {
  /**
   * TODO: Resample particles with replacement with probability proportional 
   *   to their weight. 
   * NOTE: You may find std::discrete_distribution helpful here.
   *   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
   */

   vector<double> p_weights;
    for (int i = 0; i < particles.size(); i++) {
        p_weights.push_back(particles[i].weight);
    }

   default_random_engine gen2;
   discrete_distribution<int> distribution(p_weights.begin(), p_weights.end());

   vector<Particle> resampled_particles;

   for(int i = 0; i < num_particles; i++){
      int pick = distribution(gen2);
      resampled_particles.push_back(particles[pick]);
   }

   particles = resampled_particles;
}

void ParticleFilter::SetAssociations(Particle& particle, 
                                     const vector<int>& associations, 
                                     const vector<double>& sense_x, 
                                     const vector<double>& sense_y) {
  // particle: the particle to which assign each listed association, 
  //   and association's (x,y) world coordinates mapping
  // associations: The landmark id that goes along with each listed association
  // sense_x: the associations x mapping already converted to world coordinates
  // sense_y: the associations y mapping already converted to world coordinates
  particle.associations= associations;
  particle.sense_x = sense_x;
  particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best) {
  vector<int> v = best.associations;
  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}

string ParticleFilter::getSenseCoord(Particle best, string coord) {
  vector<double> v;

  if (coord == "X") {
    v = best.sense_x;
  } else {
    v = best.sense_y;
  }

  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<float>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}