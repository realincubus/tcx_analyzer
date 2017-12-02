
#include <experimental/filesystem>
#include <pugixml.hpp>
#include <iostream>

using namespace std;
namespace fs=std::experimental::filesystem;

struct TrackPoint{
  int heart_rate;
  double speed; // m/s
  int cadence; // steps/min
};

auto analyze_trackpoint( pugi::xml_node trackpoint_node ) {
  TrackPoint tp;
  if ( auto heart_rate = trackpoint_node.child("HeartRateBpm") ) {
    auto value_node = heart_rate.child("Value");
    //cout << value_node.child_value() << " " ;
    tp.heart_rate = stoi(value_node.child_value());
  }

  if ( auto extensions = trackpoint_node.child("Extensions") ) {
    if ( auto tpx = extensions.child("TPX") ) {
      if ( auto speed = tpx.child("Speed") ) {
        //cout << stod(speed.child_value())*3.6 << " " ; 
        tp.speed = stod( speed.child_value() );
      }
      if ( auto cadence = tpx.child("RunCadence") ) {
        //cout << cadence.child_value() << " "; 
        tp.cadence = stoi( cadence.child_value() );
      }
    }
  }
  //cout << endl;
  return tp;
}

auto map_to_heart_rate_zones(const vector<TrackPoint>& track_points, const vector<std::pair<int,int>>& heart_rate_zones ) {
  vector<int> zone_bins( heart_rate_zones.size() );
  vector<double> speed_in_zone( heart_rate_zones.size() );
  for( auto& tp : track_points ){
    for (int i = 0; i < heart_rate_zones.size(); ++i){
      auto& zone = heart_rate_zones[i];
      if ( tp.heart_rate > zone.first && tp.heart_rate < zone.second ) {
        zone_bins[i]++;
        speed_in_zone[i] += tp.speed;
      }  
    }
  }
#if 0
  cout << "points in zone: " << endl;
  for( auto& zone : zone_bins ){
    cout << zone << " ";
  }
  cout << endl;
#endif
  for (int i = 0; i < speed_in_zone.size(); ++i){
     auto counts = zone_bins[i];
     auto& speed = speed_in_zone[i];
     speed = speed / counts * 3.6;
     cout << speed << " ";
  }
  cout << endl;
  return speed_in_zone;
}

auto parse_time( std::string time_string) {
  stringstream time_stream( time_string );

  std::string date_part;
  std::string time_part;

  getline( time_stream, date_part, 'T' );
  getline( time_stream, time_part, 'Z' );

  {
    stringstream date_stream( date_part );
    std::string year, month, day;
    getline( date_stream, year, '-' );
    getline( date_stream, month, '-' );
    getline( date_stream, day, '-' );
  }

  {
    stringstream time_stream( time_part );
    std::string hour, minute, second, milli_second;
    getline( time_stream, hour, ':' );
    getline( time_stream, minute, ':' );
    getline( time_stream, second, '.' );
    getline( time_stream, milli_second, 'Z' );
  }

}

auto analyze_running_activity( pugi::xml_node activity_node, const vector<std::pair<int,int>>& heart_rate_zones ) {

  vector<TrackPoint> track_points;

  for (auto lap = activity_node.child("Lap"); lap; lap = lap.next_sibling("Lap")){
    auto track = lap.child("Track");

    if ( auto start_time = lap.attribute("StartTime").value() ) {
      parse_time( start_time );
    }

    for( auto track_point = track.child("Trackpoint"); track_point; track_point = track_point.next_sibling("Trackpoint") ) {
      auto tp = analyze_trackpoint( track_point );
      track_points.push_back( tp );
    }
  }

  auto speed_in_zone = map_to_heart_rate_zones(track_points, heart_rate_zones);

  return speed_in_zone;
}

auto analyze_tcx_file( fs::path file, vector<std::pair<int,int>>& heart_rate_zones, std::vector<double>& max_speed_in_zones ) {
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(file.c_str());


  //doc.print(cout);
  auto activity_node = doc.child("TrainingCenterDatabase").child("Activities").child("Activity");


  std::string activity_type = activity_node.attribute("Sport").value();
  if ( activity_type == "Running" ){
    cout << file.filename().string() << " ";
    //cout << "is a running activity" << endl;
    auto speed_in_zone = analyze_running_activity( activity_node, heart_rate_zones ); 
    for (int i = 0; i < speed_in_zone.size(); ++i){
      auto& speed = speed_in_zone[i];
      auto& max_speed = max_speed_in_zones[i];
      if ( speed > max_speed ) max_speed = speed;
    }
    
  }else{
    //cout << "is a " << activity_type << " type Sport " << endl;
  }

 
  
  return 0;
}

int main(int argc, char** argv){

  fs::path base_dir( argv[1] );

  vector<std::pair<int,int>> heart_rate_zones;
  heart_rate_zones.emplace_back( 0 , 104 );  // 0 
  heart_rate_zones.emplace_back( 104 , 128 ); // 1 
  heart_rate_zones.emplace_back( 128 , 142 ); // 2
  heart_rate_zones.emplace_back( 142 , 152 ); // 3 
  heart_rate_zones.emplace_back( 152 , 160 ); // 4 
  heart_rate_zones.emplace_back( 160 , 255 ); // 5 

  std::vector<double> max_speed_in_zone(heart_rate_zones.size());
  std::fill( begin(max_speed_in_zone), end(max_speed_in_zone), std::numeric_limits<double>::lowest() );

  for( auto& entry : fs::directory_iterator(base_dir) ) {
    if ( fs::is_regular_file( entry ) ) {
      analyze_tcx_file( entry.path(), heart_rate_zones, max_speed_in_zone );
    }
  }
  
  std::cout << "max speed in zone" << std::endl;
  for( auto& max_speed : max_speed_in_zone ){
      cout << max_speed << " ";
  }
  cout << endl;
  

  return 0;
}
