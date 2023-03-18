sudo kill `pgrep openocd`
sudo openocd -f interface/picoprobe.cfg -f target/rp2040.cfg -c "program ./vrr_test_single_address.elf verify reset exit"
