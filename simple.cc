#include <uhd/utils/thread.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/exception.hpp>
#include <uhd/types/tune_request.hpp>
#include <complex>
#include <iostream>
#include <fstream>
#include <csignal>


static bool stop_signal_called = false;
void sig_int_handler(int)
{
    stop_signal_called = true; // Handles Ctrl+C by changing variable.
	std::cout << std::endl << "Streaming stopped." << std::endl;
}

int main(int argc, char *argv[]) {
	std::signal(SIGINT, &sig_int_handler); // Sends Ctrl+C to signal handler.

	std::string device_args("addr=192.168.40.2"); 
	uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(device_args);
	
	std::string ref("internal");
	usrp->set_clock_source(ref);
	
	std::string subdev("A:0");
	usrp->set_rx_subdev_spec(subdev);

	int rate(1e6); 
	if (1.0*rate <= 0.0) {
        	std::cerr << "Please specify a valid sample rate." << std::endl;
        	return ~0;
    	}
	usrp->set_rx_rate(1.0*rate);
	
	double freq(915e6); // Sets center frequency to tune daughterboard.
	uhd::tune_request_t tune_request(freq);
    usrp->set_rx_freq(tune_request);

	double gain(10); // Sets gain in dB.
	usrp->set_rx_gain(gain);
	
	double bw(1e6); // Sets bandwidth.
	usrp->set_rx_bandwidth(bw);

	std::string ant("RX2"); // The two antennae on A are "TX/RX" and "RX2"
	usrp->set_rx_antenna(ant);
	
	usrp->set_time_now(uhd::time_spec_t(0.0));

	uhd::stream_args_t stream_args("fc32", "sc16");
	stream_args.channels={0};
	uhd::rx_streamer::sptr rx_stream = usrp->get_rx_stream(stream_args);

	double seconds_in_future(1.5); // How many seconds to wait before receiving.

	uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
	stream_cmd.stream_now = false;
	stream_cmd.time_spec = uhd::time_spec_t(seconds_in_future);
	rx_stream->issue_stream_cmd(stream_cmd);

	uhd::rx_metadata_t md;
	std::vector<std::complex<float>> buff(rx_stream->get_max_num_samps());
	std::vector<void*> buffs;
	for (size_t ch = 0; ch < rx_stream->get_num_channels(); ch++)
		buffs.push_back(&buff.front());


	double timeout = seconds_in_future + 0.1;
	size_t num_acc_samps = 0;

	std::ofstream file;
	file.open("samples.csv");

	std::cout << "Streaming..." << std::endl;

	while (not stop_signal_called) {
		size_t num_rx_samps = rx_stream->recv(buffs, buff.size(), md, timeout, true);
		const auto now = std::chrono::steady_clock::now();
		
		timeout = 0.1;
		
		if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT)
            		break;
        	
		if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE) {
            		throw std::runtime_error(md.strerror());
        	}
		
		num_acc_samps += num_rx_samps; 

		for (auto &sample:buff) {
			file << sample.real() << "," << sample.imag() << "\n";
		}
		
	}

	stream_cmd.stream_mode = uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS;
	file.close();

	std::cout << std::endl << "Done!" << std::endl << std::endl;

	return EXIT_SUCCESS;
}
