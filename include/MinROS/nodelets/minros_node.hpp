// *****************************************************************************
//
// © Copyright 2020, Septentrio NV/SA.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//    1. Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//    2. Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//    3. Neither the name of the copyright holder nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE. 
//
// *****************************************************************************

// *****************************************************************************
//
// Boost Software License - Version 1.0 - August 17th, 2003
// 
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:

// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//
// *****************************************************************************

#ifndef MINROS_NODE_HPP
#define MINROS_NODE_HPP

/**
 * @file minros_node.hpp
 * @date 21/08/20
 * @brief The heart of the MinROS driver: The ROS node that represents it
 */
// ROS includes
#include <ros/ros.h>
#include <ros/console.h>
// ROS messages
#include <nmea_msgs/Gpgga.h>
// Boost includes
#include <boost/regex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
// The class boost::posix_time::ptime that we will use defines a location-independent time. It uses the type boost::gregorian::date, yet also stores a time.
// File includes
#include <MinROS/communication/communication_core.hpp>
#include <boost/thread/mutex.hpp>

/**
 * @namespace minros_node
 * This namespace is for the MinROS node, handling all aspects regarding
 * ROS parameters, ROS message publishing etc.
 */
namespace minros_node 
{
	//! Queue size for ROS publishers
	constexpr static uint32_t ROSQueueSize = 1;
	//! Default period in seconds between the polling of two consecutive PVTGeodetic, PosCovGeodetic, PVTCartesian and PosCovCartesian blocks and - if published - between the publishing of two of the corresponding ROS messages 
	constexpr static float poll_pub_pvt_period = 0.05;
	//! Default period in seconds between the polling of two consecutive AttEuler, AttCovEuler blocks as well as the HRP NMEA sentence, and - if published - between the publishing of AttEuler and AttCovEuler
	constexpr static float poll_pub_orientation_period = 0.05;
	//! Default period in seconds between the polling of all other SBF blocks and NMEA sentences not addressed by the previous two ROS parameters, and - if published - between the publishing of all other ROS messages 
	constexpr static float poll_pub_rest_period = 0.05;

	//! Node Handle for minros node
	//! You must initialize the NodeHandle in the "main" function (or in any method called indirectly or directly by the main function). One can declare a pointer to the NodeHandle to be a global variable and then initialize it afterwards only...
	boost::shared_ptr<ros::NodeHandle> nh;
	//! Handles communication with the mosaic
	io_comm_mosaic::Comm_IO IO;
	//! Whether or not to publish the given mosaic message
	/**
	 * The key is the message name, i.e. the message ID for SBF blocks embedded in inverted commas (a string) or the message ID for NMEA messages.
	 * The value indicates whether or not to enable that message. 
	 */
	std::map<std::string, bool> enabled;	
	
	/**
	 * @brief Determines current time via BOOST, using the type boost::gregorian::date
	 * @return Total number of seconds since midnight
	 */
	double defaultGetTimeHandler() 
	{
		boost::posix_time::ptime present_time(boost::posix_time::microsec_clock::universal_time());
		boost::posix_time::time_duration duration(present_time.time_of_day());
		return duration.total_seconds();
	}
	
	/**
	 * @brief Publishes a ROS message of type MessageT to topic "topic".
	 * @param m The message to publish
	 * @param topic The topic to publish the message to
	 */
	template <typename MessageT>
	void publish(const MessageT& m, const std::string& topic) 
	{
		//ROS_DEBUG("About to publish message");
		// long read_timestamp = defaultGetTimeHandler(); 
		// cpu timestamp could be used for whatever purposes, otherwise you get unused warning from compiler
		static ros::Publisher publisher = nh->advertise<MessageT>(topic, ROSQueueSize);
		publisher.publish(m);
	}
	
	
	/**
	 * @brief Checks whether the parameter is in the given range
	 * @param val The value to check
	 * @param min The minimum for this value
	 * @param max The maximum for this value
	 * @param name The name of the parameter
	 * @throws std::runtime_error if it is out of bounds
	 */
	template <typename V, typename T>
	void checkRange(V val, T min, T max, std::string name) 
	{
		if(val < min || val > max) 
		{
			std::stringstream oss;
			oss << "Invalid settings: " << name << " must be in range [" << min <<
				", " << max << "].";
			throw std::runtime_error(oss.str());
		}
	}

	/**
	 * @brief Check whether the elements of the vector are in the given range
	 * @param[in] val The vector to check
	 * @param[in] min The minimum for this value
	 * @param[in] max The maximum for this value
	 * @param[in] name The name of the parameter
	 * @throws std::runtime_error value if it is out of bounds
	 */
	template <typename V, typename T>
	void checkRange(std::vector<V> val, T min, T max, std::string name) 
	{
		for(size_t i = 0; i < val.size(); i++)  
		{
			std::stringstream oss;
			oss << name << "[" << i << "]";
			checkRange(val[i], min, max, oss.str());
		}
	}
	
	/**
	 * @brief Gets an integer or unsigned integer value from the parameter server
	 * @param[in] key The key to be used in the parameter server's dictionary
	 * @param[out] u Storage for the retrieved value, of type U, which can be either unsigned int or int
	 * @throws std::runtime_error if the parameter is out of bounds
	 * @return True if found, false if not found
	 */
	template <typename U>
	bool GetROSInt(const std::string& key, U &u) {
		int param;
		if (!nh->getParam(key, param)) return false;
		// Check the bounds
		U min = std::numeric_limits<U>::lowest();
		U max = std::numeric_limits<U>::max();
		checkRange((U) param, min, max, key);
		// set the output
		u = (U) param;
		return true;
	}

	/**
	 * @brief Gets an integer or unsigned integer value from the parameter server
	 * @param[in] key The key to be used in the parameter server's dictionary
	 * @param[out] u Storage for the retrieved value, of type U, which can be either unsigned int or int
	 * @param[in] val Value to use if the server doesn't contain this parameter
	 * @throws std::runtime_error if the parameter is out of bounds
	 * @return True if found, false if not found
	 */
	template <typename U, typename V>
	void GetROSInt(const std::string& key, U &u, V default_val) 
	{
		if(!GetROSInt(key, u))
			u = default_val;
	}

	/**
	 * @brief Gets an unsigned integer or integer vector from the parameter server
	 * @throws std::runtime_error if the parameter is out of bounds
	 * @param[in] key The key to be used in the parameter server's dictionary
	 * @param[out] u Storage for the retrieved value, of type std::vector<U>, where U can be either unsigned int or int
	 * @return True if found, false if not found
	 */
	template <typename U>
	bool GetROSInt(const std::string& key, std::vector<U> &u) 
	{
		std::vector<int> param;
		if (!nh->getParam(key, param)) return false;

		// Check the bounds
		U min = std::numeric_limits<U>::lowest();
		U max = std::numeric_limits<U>::max();
		checkRange(param, min, max, key);

		// set the output
		u.insert(u.begin(), param.begin(), param.end());
		return true;
	}

	
	/**
	 * @class MinROSNode
	 * @brief This class represents the MinROS node for mosaic-X5, to be extended..
	 */
	class MinROSNode
	{
		public:
		
			//! The constructor initializes and runs the MinROS node, if all works out fine.
			//! It loads the user-defined ROS parameters, subscribes to mosaic messages, and publishes requested ROS messages (for now, 1-1 or GGA-like, not yet NavSatFix that needs multiple messages as input..)
			MinROSNode();
			
			/**
			 * @brief Get the node parameters from the ROS Parameter Server, parts of which are specified in a YAML file, other parts of which are specified via the command line.
			 */
			void GetROSParams();
			
			/**
			 * @brief Subscribe (= read in) to all requested mosaic messages and publish them
			 */
			void Subscribe();
			
			/**
			 * @brief Initialize the I/O handling.
			 */
			void InitializeIO();
			
			/**
			 * @brief Attempts to (re)connect every reconnect_delay_s_ seconds
			 */
			void Reconnect(const ros::TimerEvent& event);

			
		private:
			//! Device port
			std::string device_;
			//! Baudrate
			uint32_t baudrate_;
			//! Delay in seconds between reconnection attempts to the connection type specified in the parameter connection_type
			float reconnect_delay_s_;
			//! Our ROS timer governing the reconnection
			ros::Timer reconnect_timer_;
			//! Whether or not connection has been successful so far
			bool connected_ = false;
			//! Whether or not to publsh GGA messages
			bool publish_gpgga_;
	};
}

#endif // for MINROS_NODE_HPP