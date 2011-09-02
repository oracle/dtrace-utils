CPU     ID                    FUNCTION:NAME
                                 :tick-1sec   blastoff!                        
          
  quantized countdown                               
           value  ------------- Distribution ------------- count    
              -1 |                                         0        
               0 |@@@@                                     1        
               1 |                                         0        
               2 |@@@@                                     1        
               4 |@@@@@@@@                                 2        
               8 |@@@@@@@@@@@@@@@@                         4        
              16 |@@@@@@@@                                 2        
              32 |                                         0        

  linear quantized countdown                        
           value  ------------- Distribution ------------- count    
             < 0 |                                         0        
               0 |@@@@                                     1        
               1 |                                         0        
               2 |@@@@                                     1        
               3 |                                         0        
               4 |@@@@                                     1        
               5 |                                         0        
               6 |@@@@                                     1        
               7 |                                         0        
               8 |@@@@                                     1        
               9 |                                         0        
              10 |@@@@                                     1        
              11 |                                         0        
              12 |@@@@                                     1        
              13 |                                         0        
              14 |@@@@                                     1        
              15 |                                         0        
              16 |@@@@                                     1        
              17 |                                         0        
              18 |@@@@                                     1        
              19 |                                         0        

-- @@stderr --
dtrace: script 'test/intro/countdown-quant.d' matched 3 probes
