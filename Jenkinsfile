#!/usr/bin/env groovy

// Builds options on UNIX.
unixOpts = "-DCAF_NO_PROTOBUF_EXAMPLES:BOOL=yes " +
           "-DCAF_NO_QT_EXAMPLES:BOOL=yes " +
           "-DCAF_MORE_WARNINGS:BOOL=yes" +
           "-DCAF_ENABLE_ADDRESS_SANITIZER:BOOL=yes" +
           "-DCAF_ENABLE_RUNTIME_CHECKS:BOOL=yes" +
           "-DCAF_USE_ASIO:BOOL=yes" +
           "-DCAF_NO_BENCHMARKS:BOOL=yes" +
           "-DOPENSSL_ROOT_DIR=/usr/local/opt/openssl" +
           "-DOPENSSL_INCLUDE_DIR=/usr/local/opt/openssl"

// Builds options on Windows.
msOpts = "-DCAF_BUILD_STATIC_ONLY:BOOL=yes " +
         "-DCAF_NO_BENCHMARKS:BOOL=yes " +
         "-DCAF_NO_EXAMPLES:BOOL=yes " +
         "-DCAF_NO_MEM_MANAGEMENT:BOOL=yes " +
         "-DCAF_NO_OPENCL:BOOL=yes " +
         "-DCAF_LOG_LEVEL:INT=0 " +
         "-DCMAKE_CXX_FLAGS=\"/MP\""

pipeline {
  agent none
  environment {
    LD_LIBRARY_PATH = "$WORKSPACE/caf-sources/build/lib"
    DYLD_LIBRARY_PATH = "$WORKSPACE/caf-sources/build/lib"
    ASAN_OPTIONS = 'detect_leaks=1'
  }
  stages {
    stage ('Git Checkout') {
      steps {
        node ('master') {
          deleteDir()
          dir('caf-sources') {
            checkout scm
          }
          stash includes: 'caf-sources/**', name: 'caf-sources'
        }
      }
    }
    stage ('Build & Test') {
      parallel {
        stage ('GCC 4.8') {
          agent { label 'Linux && gcc4.8' }
          steps { unixBuild() }
        }
        stage ('GCC 4.9') {
          agent { label 'Linux && gcc4.9' }
          steps { unixBuild() }
        }
        stage ('GCC 5') {
          agent { label 'Linux && gcc5.1' }
          steps { unixBuild() }
        }
        stage ('GCC 6') {
          agent { label 'Linux && gcc6.3' }
          steps { unixBuild() }
        }
        stage ('GCC 7') {
          agent { label 'Linux && gcc7.2' }
          steps { unixBuild() }
        }
        stage ('Clang on Linux') {
          agent { label 'Linux && clang' }
          steps { unixBuild() }
        }
        stage ('Clang on Mac') {
          agent { label 'macOS && clang' }
          steps { unixBuild() }
        }
        stage ('GCC on Mac') {
          agent { label 'macOS && gcc' }
          steps { unixBuild() }
        }
        stage('Leak Sanitizer') {
          agent { label 'Linux && LeakSanitizer' }
          steps { unixBuild() }
        }
        stage('Logging') {
          agent { label 'Linux' }
          steps { unixBuild([buildOpts: '-D CAF_LOG_LEVEL=4']) }
        }
        stage('Release') {
          agent { label 'Linux' }
          steps { unixBuild('Release') }
        }
        stage('Coverage') {
          agent { label "gcovr" }
          steps {
            unixBuild([buildOpts: '-D CAF_ENABLE_GCOV=yes'])
            dir('caf-sources') {
              sh 'gcovr -e libcaf_test -e ".*/test/.*" -x -r .. > coverage.xml'
              cobertura([
                autoUpdateHealth: false,
                autoUpdateStability: false,
                coberturaReportFile: '**/coverage.xml',
                conditionalCoverageTargets: '70, 0, 0',
                failUnhealthy: false,
                failUnstable: false,
                lineCoverageTargets: '80, 0, 0',
                maxNumberOfBuilds: 0,
                methodCoverageTargets: '80, 0, 0',
                onlyStable: false,
                sourceEncoding: 'ASCII',
                zoomCoverageChart: false,
              ])
            }
          }
        }
        stage('Windows (MSVC)') {
          agent { label "msbuild" }
          environment {
            PATH = 'C:\\Windows\\System32;C:\\Program Files\\CMake\\bin;C:\\Program Files\\Git\\cmd;C:\\Program Files\\Git\\bin'
          }
          steps { msBuild() }
        }
      }
    }
  }
  post {
    success {
      echo "Success."
      // TODO: Gitter?
    }
    failure {
      echo "Failure, sending mails!"
      // TODO: Gitter?
      emailext(
        subject: "FAILED: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]'",
        body: """FAILED: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]':
                 Check console output at "${env.BUILD_URL}"
                 Job: ${env.JOB_NAME}
                 Build number: ${env.BUILD_NUMBER}""",
        // TODO: Uncomment the following line:
        // recipientProviders: [[$class: 'CulpritsRecipientProvider'], [$class: 'RequesterRecipientProvider']]
        to: 'hiesgen;neverlord' // add multiple separated with ';'
      )
    }
  }
}

def unixBuild(buildType = 'Debug',
              generator = 'Unix Makefiles',
              buildOpts = '',
              cleanBuild = true) {
  deleteDir()
  unstash('caf-sources')
  dir('caf-sources') {
    // Configure and build.
    cmakeBuild([
      buildDir: 'build',
      buildType: "$buildType",
      cleanBuild: cleanBuild,
      cmakeArgs: "$unixOpts $buildOpts",
      generator: "$generator",
      installation: 'cmake in search path',
      preloadScript: '../cmake/jenkins.cmake',
      sourceDir: '.',
      steps: [[args: 'all']],
    ])
    // Test.
    ctest([
      arguments: '--output-on-failure',
      installation: 'cmake in search path',
      workingDir: 'build',
    ])
  }
}

def msBuild(buildType = 'Debug',
            generator = 'Visual Studio 15 2017',
            buildOpts = '',
            cleanBuild = true) {
  deleteDir()
  unstash('caf-sources')
  dir('caf-sources') {
    // Configure and build.
    // installation can be either 'cmake auto install' or 'cmake in search path'
    // cmakeBuild buildDir: 'build', buildType: "$buildType", cleanBuild: cleanBuild, cmakeArgs: "$ms_opts", generator: "$generator", installation: 'cmake in search path', preloadScript: '../cmake/jenkins.cmake', sourceDir: '.', steps: [[args: 'all']]
    def ret = bat(returnStatus: true,
              script: """cmake -E make_directory build
                         cd build
                         cmake -DCMAKE_buildType=$buildType -G "$generator" $msOpts ..
                         IF /I "%ERRORLEVEL%" NEQ "0" (
                           EXIT 1
                         )
                         cat build/CMakeCache.txt
                         EXIT 0""")
    if (ret) {
      echo "[!!!] Configure failed!"
      currentBuild.result = 'FAILURE'
      return
    }
    // bat "echo \"Step: Build for '${tags}'\""
    ret = bat(returnStatus: true,
              script: """cd build
                         cmake --build .
                         IF /I "%ERRORLEVEL%" NEQ "0" (
                           EXIT 1
                         )
                         EXIT 0""")
    if (ret) {
      echo "[!!!] Build failed!"
      currentBuild.result = 'FAILURE'
      return
    }
    // Test.
    ctest([
      arguments: '--output-on-failure',
      installation: 'cmake auto install',
      workingDir: 'build',
    ])
  }
}
