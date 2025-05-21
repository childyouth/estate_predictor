# Estate_predictor
https://www.data.go.kr/data/15126474/openapi.do#/layer-api-guide  
부동산원 자료 사용한 예측 프로그램  

## 구현 방향
estate_parser : 부동산 데이터 파싱

이후  
spark 사용, 데이터 전처리  
pytorch 사용, 부동산 예측  

## .ini 예시 
```
[key]
api_rent_key = 
api_trade_key = 

[api_addr]
api_base_addr = apis.data.go.kr
api_rent_endpoint = /1613000/RTMSDataSvcAptRent/getRTMSDataSvcAptRent
api_trade_endpoint = /1613000/RTMSDataSvcAptTrade/getRTMSDataSvcAptTrade

[variable]
earliest_ymd = 201101
latest_ymd = 202504

[engine]
worker_max_workload = 256
num_thread = 8
savename = 
stdcode_filename = ./stdcode_only.bin
```
