config = {
    "header": {
        "uuid": "b5e2cef1-a875-4f37-bdb7-bdc273411f7d",
        "type": "SEC_INPUT",
        "vendor": "Touchscreen",
        "product": "FOLDABLE",
        "variant": "stm_spi",
        "name": "stm_spi",
    },
    "build": {
        "path": "input/sec_input/touchscreen/stm_spi",
        "file": "input_stm_spi.py",
        "location": [
            {
                "depend": "feature['General']",
                "dst": "drivers/input/sec_input/stm_spi/",
                "src": "*.c *.h Kconfig Makefile",
            },
            {
                "depend": "feature['AP'] == 'slsi'",
                "dst": "arch/arm64/boot/dts/samsung/input_stm_spi.dtsi",
                "src": "dts/input-lsi-tsp-stm.dtsi",
            },
            {
                "depend": "feature['AP'] == 'qcom'",
                "dst": "arch/arm64/boot/dts/samsung/input_stm_spi.dtsi",
                "src": "dts/input-qc-tsp-stm.dtsi",
            },
            {
                "depend": "feature['AP'] == 'mtk'",
                "dst": "arch/arm64/boot/dts/samsung/input_stm_spi.dtsi",
                "src": "dts/input-mtk-tsp-stm.dtsi",
            },
            {
                "depend": "feature['AP'] == 'mtk'",
                "dst": "firmware/tsp_stm/",
                "src": "firmware/*.bin",
            },
            {
                "depend": "feature['AP'] == 'qcom'",
                "dst": "firmware/tsp_stm/",
                "src": "firmware/*.bin",
            },
        ],
    },
    "features": [
        {
            "configs": {
                "False": [],
                "True": [],
            },
            "label": "General",
            "type": "boolean",
        },
        {
        
            "configs": {
                "built-in": [
                    "CONFIG_TOUCHSCREEN_STM_SPI=y"
                ],
                "module": [
                    "CONFIG_TOUCHSCREEN_STM_SPI=m"
                ],
                "not set": [
                    "# CONFIG_TOUCHSCREEN_STM_SPI is not set"
                ]
            },
            "label": "STM-Config-Option",
            "list_value": [
                "not set",
                "module",
                "built-in"
            ],
            "type": "list",
        },
        {
        
            "configs": {
                "set FTS2BA61Y": [
                    "CONFIG_TOUCHSCREEN_STM_SPI_FTS2BA61Y=y"
                ],
                "not set": [
                    "# CONFIG_TOUCHSCREEN_STM_SPI_FTS2BA61Y is not set"
                ]
            },
            "label": "Select IC",
            "list_value": [
                "not set",
                "set FTS2BA61Y",
            ],
            "type": "list",
            "value": "not set",
        },
        {
            "label": "AP",
            "list_value": [
                "slsi",
                "qcom",
                "mtk",
            ],
            "type": "list",
        },
        {
            "label": "Model Name",
            "symbol": "${model_name}",
            "type": "string",
        },
	{
            "label": "bringup",
            "symbol": "${bringup}",
            "type": "string",
        },
    ],
    "interfaces": {
	    "inports": [
	    {
		    "type": "gpio",
		    "labels": [
		    {
			    "label": "GPIO: INT",
			    "properties": [],
			    "symbol": "${gpio_int}",
			    "parent": "${gpio_int_parent}",
		    },
		    {
			    "label": "GPIO: MOSI",
			    "properties": [],
			    "symbol": "${gpio_spi_mosi}",
			    "parent": "${gpio_spi_parent}",
		    },
		    {
			    "label": "GPIO: MISO",
			    "properties": [],
			    "symbol": "${gpio_spi_miso}",
			    "parent": "${gpio_spi_parent}",
		    },
		    {
			    "label": "GPIO: CLK",
			    "properties": [],
			    "symbol": "${gpio_spi_clk}",
			    "parent": "${gpio_spi_parent}",
		    },
		    {
			    "label": "GPIO: CS",
			    "properties": [],
			    "symbol": "${gpio_spi_cs}",
			    "parent": "${gpio_spi_parent}",
		    },
		    ],
	    },
            {
                "labels": [
                    {
                        "label": "SPI INF",
                        "properties": [],
                        "symbol": "${inf_spi}",
                    }
                ],
                "type": "spi",
            },
            {
                "labels": [
                    {
                        "label": "LDO: TSP IO",
                        "properties": [],
                        "symbol": "${tsp_io_ldo}",
                    },
                    {
                        "label": "LDO: TSP AVDD",
                        "properties": [],
                        "symbol": "${tsp_avdd_ldo}",
                    },
                ],
                "type": "regulator",
            },
        ]
    },
}


def load():
    return config

if __name__ == '__main__':
    print(load())
