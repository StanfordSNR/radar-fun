#include <complex>
#include <csignal>
#include <fstream>
#include <iostream>
#include <string>
#include <uhd/exception.hpp>
#include <uhd/types/tune_request.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/utils/thread.hpp>

using namespace std;

static bool stop_signal_called = false;
void sig_int_handler( int )
{
  stop_signal_called = true; // Handles Ctrl+C by changing variable.
}

int main()
{
  signal( SIGINT, &sig_int_handler ); // Sends Ctrl+C to signal handler.

  auto usrp = uhd::usrp::multi_usrp::make( "addr=192.168.40.2" );

  usrp->set_clock_source( "internal" );
  usrp->set_rx_subdev_spec( "A:0"s ); // select daughterboard
  usrp->set_rx_rate( 1e6 );           // samples per second
  usrp->set_rx_freq( 1e5 );           // center frequency
  usrp->set_rx_gain( 10 );            // gain in dB
  usrp->set_rx_bandwidth( 5e4 );      // bandwidth in Hz
  usrp->set_rx_antenna( "RX2" );      // the two antennas on A are "TX/RX" and "RX2"
  usrp->set_time_now( 0.0 );          // align internal clock

  // create receive stream
  uhd::stream_args_t stream_args { "fc32", "sc16" };
  stream_args.channels = { 0 };
  auto rx_stream = usrp->get_rx_stream( stream_args );

  // start streaming
  rx_stream->issue_stream_cmd( uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS );

  uhd::rx_metadata_t metadata;
  vector<complex<float>> buff( rx_stream->get_max_num_samps() );
  vector<void*> buffs { buff.data() };

  size_t num_acc_samps = 0; // total number of accumulated samples
  size_t next_status_update = 0;

  ofstream file;
  file.open( "samples.csv" );

  cout << "Streaming..." << endl;

  while ( not stop_signal_called ) {
    size_t num_rx_samps = rx_stream->recv( buffs, buff.size(), metadata, 5, true );

    if ( metadata.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE ) {
      throw runtime_error( metadata.strerror() );
    }

    num_acc_samps += num_rx_samps;

    for ( size_t i = 0; i < num_rx_samps; ++i ) {
      const auto& sample = buff.at( i );
      file << sample.real() << "," << sample.imag() << "\n";
    }

    if ( num_acc_samps >= next_status_update ) {
      cerr << "\rSamples received: " << num_acc_samps << "\033[K";
      next_status_update = num_acc_samps + 1e5; // update every 100 ms
    }
  }

  rx_stream->issue_stream_cmd( uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS );
  file.close();

  cout << "\nDone!\n";

  return EXIT_SUCCESS;
}
