#include <uhd/utils/thread.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/exception.hpp>
#include <uhd/types/tune_request.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <complex>
#include <iostream>
#include <fstream>
#include <typeinfo>
#include <chrono>

static bool stop_signal_called = false;
void sig_int_handler(int)
{
    stop_signal_called = true;
}

void vector_fill(std::vector<std::complex<float>>& big, \
    std::vector<std::complex<float>> small, \
    int B, int S, int start_index) {
        for (int i = start_index; i < std::min(B,start_index+S); ++i) {
            big[i] += small[i - start_index];
        }

        if (B < start_index + S) {
            for (int i = 0; i < (start_index + S) % B; ++i) {
                big[i] += small[B - start_index + i];
            }
        }
}

// UHD_SAFE_MAIN is a catch-all wrapper of main. Why in this syntax??
int UHD_SAFE_MAIN(int argc, char *argv[]) {
	uhd::set_thread_priority_safe();
		// Sets "scheduling priority" ??
		// Documentation says this is identical to "set_thread_priority" 
		// but doesn't throw in case of failure
	


	std::string device_args("addr=192.168.40.2"); 
	uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(device_args);
	std::cout << std::endl;
    	std::cout << boost::format("Made Multi USRP with IP: %s...") % device_args << std::endl;
		// Set IP address of USRP in format required to "make device"
		// Can find this from uhd_find_devices in terminal


	std::string ref("internal");
	std::cout << boost::format("Source of motherboard clock: %f...") % ref << std::endl;
    	usrp->set_clock_source(ref);
		// This sets the source for a reference clock that is default 10 MHz.
		// Can be set to "internal", "external", and multiple-in-multiple-out "MIMO".
	
	// Intereresting documentation about the clock: In the x310, the Digital
	// Down-Converter and Up-Converter (used in DAC and ADC) run on the same
	// clock as the "header passing packets." This allows timed commands to 
	// be executed with extreme precision. However, this also limits the
	// amount of commands that can be stored in the command queue. If the
	// device is not streaming the data simultaneously, adding commands can 
	// shut down the device. Managing the DDC and DUC queue will be vital to
	// continuous receiving and transmitting of signals.


	std::string subdev("A:0");
	usrp->set_rx_subdev_spec(subdev);
	std::cout << boost::format("Using Device: %s") % usrp->get_pp_string() << std::endl;
		// Format for setting the sub-device: the daughterboard inside of the radio
		// which communicates with the antenna. The format is 
			// <motherboard slot>:<daughterboard name>
		// Here, we are using the RF A motherboard slot (where the antennas are plugged
		// in) and setting the daughterboard frontend name to 0. You can check possible
		// daughterboard frontend names for specific boards by probing device configuration
		// with uhd_usrp_probe. The last line prints the device configuration.


	int rate(1e6); // The Rx sampling rate in Samples/second.
	if (1.0*rate <= 0.0) {
        	std::cerr << "Please specify a valid sample rate" << std::endl;
        	return ~0;
		// Ensures a positive sampling rate is selected.
    	}
	usrp->set_rx_rate(1.0*rate);
	std::cout << boost::format("Set Rx rate to: %f MS/s...") % (1.0*rate / 1e6) << std::endl;
	std::cout << boost::format("measured Rx rate: %f MS/s...") % (usrp->get_rx_rate() / 1e6) << std::endl << std::endl;
		// Sets the frequency that the received signal is sampled/stored at.
		// Allows this to be checked with the true sampling rate.
	
	
	double freq(915e6); // The Rx center frequency in Samples/second.
	uhd::tune_request_t tune_request(freq); // Tells daughterboard to tune to specific center freq.
    	usrp->set_rx_freq(tune_request);
	std::cout << boost::format("Set Rx center freq to: %f MHz...") % (freq / 1e6) << std::endl;
	std::cout << boost::format("Measured Rx center freq: %f MHz...") % (usrp->get_rx_freq() / 1e6) << std::endl << std::endl;
	

	double gain(10); // Gain in dB for RF signal.
	usrp->set_rx_gain(gain);
	std::cout << boost::format("Set Rx Gain to: %f dB...") % gain << std::endl;
	std::cout << boost::format("Measured Rx Gain to: %f dB...") % usrp->get_rx_gain() << std::endl << std::endl;

	
	double bw(1e6); // Frontend bandwidth for IF signal. 
	usrp->set_rx_bandwidth(bw);
	std::cout << boost::format("Set Rx bandwidth to: %f MHz...") % (bw / 1e6) << std::endl;
	std::cout << boost::format("Measured Rx bandwidth: %f MHz...") % (usrp->get_rx_bandwidth() / 1e6) << std::endl << std::endl;


	std::string ant("RX2"); // The two antennae on A are "TX/RX" and "RX2"
	usrp->set_rx_antenna(ant);
	std::cout << boost::format("Set Rx antenna to: %s") % ant << std::endl;
	std::cout << boost::format("Detected Rx antenna: %s") % usrp->get_rx_antenna() << std::endl << std::endl;


	std::cout << boost::format("Setting device timestamp to 0...") << std::endl;
	usrp->set_time_now(uhd::time_spec_t(0.0));
		// Synchronizes time of clock to 0.0 to allow for simultaneous 
		// commands based on same clock time.


	uhd::stream_args_t stream_args("fc32", "sc16");
		// Creates the "stream_args" object
		// Initializes to record data as 32 bit complex float
		// Initializes to pass data as 16 bit complex integer.
	stream_args.channels={0};
		// Lets you stream data over different channels. This allows for 
		// multiple channels to be streamed simultaneously. Here, the number
		// (0) indicates that we are sending data from the first subdev spec,
		// in this case "A:0". The order which these channels are received
		// by the computer are determined by the order of numbers in the set.
		// I.e. {1,0} would send data from B:0 to the application's first 
		// channel, and data from A:0 to the application's second channel.
	uhd::rx_streamer::sptr rx_stream = usrp->get_rx_stream(stream_args);
		// Creates a new "receive stream" from the arguments in "stream_args".
		// This stream must be destroyed before calling get_rx_stream again.
	std::cout << std::endl;


	double seconds_in_future(1.5); // How many seconds to wait before receiving.
	size_t total_num_samps(10001); // Total number of samples to receive.
		// It is unclear from documentation why this is set up independently
		// from the Rx Sampling Rate. Shouldn't the number of samples received
		// be determined by the time sampled and the sampling rate?
	// std::cout << boost::format("Streaming %u samples, for %f seconds...") % total_num_samps % seconds_in_future << std::endl;
	
	std::cout << boost::format("Streaming continuously after delay of %u s.") % seconds_in_future << std::endl;
	uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
		// Creates "command structure" to control streaming.
		// STREAM_MODE_START_CONTINUOUS   = start streaming continuously.
		// STREAM_MODE_STOP_CONTINUOUS 	  = stop continuous streaming.
		// STREAM_MODE_NUM_SAMPS_AND_DONE = stream exact number of samples.
		// STREAM_MODE_NUM_SAMPS_AND_MORE = stream exact # of samples and expect future command for contiguous samples.
	//stream_cmd.num_samps = total_num_samps;
	stream_cmd.stream_now = false;
	stream_cmd.time_spec = uhd::time_spec_t(seconds_in_future);
		// How many seconds the system will wait before it starts to dtream.
	rx_stream->issue_stream_cmd(stream_cmd);
		// Issues streaming commands to the designed rx_stream from earlier.

	uhd::rx_metadata_t md;
		// Configures object to store metadata describing IF data.
		// Includes time specification, fragmentation flags, burst flags, error codes.


	std::vector<std::complex<float>> buff(rx_stream->get_max_num_samps());
		// Gets the maximum number of samples per buffer per packet.
	std::vector<void*> buffs;
		// A little unsure what is happening here. This is setup in the case
		// that multiple channels are being used. (Just one is being used 
		// here.) "buffs" is pointer of writable memory to fill with samples.
		// The loop makes it so all channels are stored on the same buffer.
		// I think this can (and should be) changed in the case of threading,
		// since "recv" can't be threaded unless it is passing channels
		// individually. 
	for (size_t ch = 0; ch < rx_stream->get_num_channels(); ch++)
		buffs.push_back(&buff.front());


	double timeout = seconds_in_future + 0.1;
		// This will control the initial timeout before receiving the first
		// packet. We give 0.1 seconds of padding time for each packet.

	size_t num_acc_samps = 0;
	
	const auto start_time = std::chrono::steady_clock::now();
	const auto last_update = start_time;

	const int SPP(rx_stream->get_max_num_samps());
	const int input_period(10);
	std::vector<std::complex<float>> data(input_period*rate);
    std::fill(data.begin(), data.end(), std::complex<double>(0.0,0.0));
	int mod_start(0);
	int ct(0);

/*	std::vector<std::complex<float>> blank(SPP);
	for (int i = 0; i < SPP; ++i) {
		blank[i]=std::complex<double>(1.0*i,1.0*i);
	}
	std::cout << blank; */
	std::ofstream file;
	

	//while (num_acc_samps < total_num_samps) {
	while (not stop_signal_called) {
		
		size_t num_rx_samps = rx_stream->recv(buffs, buff.size(), md, timeout, true);
			// Receives a single packet. The recv function passes data
			// to the buffer buffs of a size "buff.size" as indicated by
			// the metadata object. It then outputs the total number of
			// samples that were passed in the packet.

		

		timeout = 0.1; // After first packet, we can set the timeout to 0.1 again.
		
		if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT)
            		break;
        	
		if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE) {
            		throw std::runtime_error(
                		str(boost::format("Receiver error %s") % md.strerror()));
        	}
			// This is how the documentation says to handle errors.
		
		num_acc_samps += num_rx_samps; 
			// Keeps track of how many total samples have been accumulated.
		


		const auto now = std::chrono::steady_clock::now();
		const auto diff = std::chrono::duration<double>(now - last_update).count();

		
		/*if (ct==2) {
			for (auto &sample:buff) {
				std::cout << sample.real() << std::endl;
			}
		}*/
		
		vector_fill(data, buff, input_period*rate, SPP, mod_start);
		mod_start += SPP;
		if (mod_start + SPP > input_period*rate-1) {
			mod_start = mod_start % input_period*rate;

			file.open("samples.csv");
			for (auto &sample:data) {
				file << sample.real() << "," << sample.imag() << "\n";
			}
			file.close();
		}
		
/*		file.open("samples.csv", std::ios_base::app);
		for (auto &sample:buff) {
			file << sample.real() << "," << sample.imag() << "\n";
		}
		file.close();*/

		ct+=1;
		std::cout << boost::format("Packet # %u has %u samples at time t = %d.") % ct % num_rx_samps % diff << std::endl;
	}
	

	// if (num_acc_samps < total_num_samps)
        	// std::cerr << "Receive timeout before all samples received..." << std::endl;
			// Prints this message if timeout happened at some point before
			// all packets were sent.

	std::cout << std::endl << "Done!" << std::endl << std::endl;

	
	
	

	return EXIT_SUCCESS;
}
